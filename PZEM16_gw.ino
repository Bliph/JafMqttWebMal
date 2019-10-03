#include "JafMqttWebMal.h"

#define SERIAL_BUFFER_LENGTH  500
#define MB_COMMAND_LENGTH     8

/*
 * 01040000000A700D   Command
 * xxxx01             Response
 * 1051               U
 * 20040000           I
 * 00000000           P
 * 00010000           E
 * 01F3               F
 * 0000               pf
 * 0000               AL
 * B872               CRC

 * 
 * 
 * 
 * PZEM A -> 485 A
 * PZEM B -> 485 B
 * 485 RO -> Data out from PZEM
 * 485 DI <- Data in to PZEM
 * 485 RE - 0V
 * 485 DE - +5V???
*/

//static const uint8_t D5   = 14;
//static const uint8_t D6   = 12;

class PZEM16_gw : public JafMqttWeb
{
//
// CRC-16-IBM
// X^16+X^15+X+1 = 0x8005
// https://en.wikipedia.org/wiki/Cyclic_redundancy_check
//
  public:
    PZEM16_gw(int led = LED_BUILTIN_NODEMCU, bool debug = false) : JafMqttWeb(led, debug) { };

  private:
    bool newDataAvailable = false;
//    byte MBcmd[8] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x0a, 0x0d, 0x70};
    byte MBcmd[8] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00};
    byte MBbuffer[SERIAL_BUFFER_LENGTH];
    int  MBbuffer_len = 0;

    int   Voltage = -10000;
    long  Current = -10000;
    long  Power = -10000;
    long  Energy = -10000;
    int   Frequency = -10000;
    int   PowerFactor = -10000;
    int   Alarm = -10000;
    int   CRC = -10000;

    // Compute the modbus CRC
    uint16_t modbusCRC(byte* data, int len)
    {
      uint16_t crc = 0xFFFF;
    
      for (int pos = 0; pos < len; pos++) 
      {
        crc ^= (uint16_t)data[pos];   // XOR byte into least sig. byte of crc
    
        for (int i = 8; i != 0; i--) 
        {                             // Loop over each bit
          if ((crc & 0x0001) != 0) 
          {                           // If the LSB is set
            crc >>= 1;                // Shift right and XOR 0xA001
            crc ^= 0xA001;
          }
          else crc >>= 1;             // Else LSB is not set, shift right
        }
      }
    
      // now reverse the bytes so it's easy to use.
      return (crc << 8) | (crc >> 8 );
    }
    
    bool modbusCRCVerify(byte* data, int len)
    {
      return modbusCRC(data, min(len, (int)(data[1] + 5))) == 0;
    }

    // Send command on serial link and read back response.
    // Returns number of bytes read on response
    
    int serialCmd(byte cmd[], int cmd_len, byte rsp[], int rsp_len)
    { 
      int n = 0;
    
      // Empty response buffer
      n = Serial.readBytes(rsp, rsp_len);
    
      // Write command
      digitalWrite(D5, HIGH);     //  DE (Driver Enable)
      digitalWrite(D6, HIGH);      // /RE (Receive Enable)
      Debug("Data OUT --> ", cmd, cmd_len);
      Serial.write(cmd, cmd_len);
      Serial.flush();
      delay(10);

      digitalWrite(D5, LOW);      //  DE (Driver Enable)
      digitalWrite(D6, LOW);      // /RE (Receive Enable)
      delay(100);
      
      for (int i = 0; (i < 10) && (Serial.available() <= 0); i++) delay(i*100);
      for (n = 0; (n < rsp_len) && Serial.available() > 0; n++) 
      {
        rsp[n] = Serial.read();
        delay(10);
      }
      
      if (modbusCRCVerify(rsp, min(rsp_len, n))) Debug("Data IN (CRC OK) <-- ", rsp, n);    
      else 
      {
        Debug("Data IN (xxxxx) <-- ", rsp, n);
        if (n >= 2)
        {
          byte pl[10];
          set16bValue(pl, modbusCRC(rsp, n-2));
          Debug("CRC: ", pl, 2);
          set16bValue(pl, modbusCRC(rsp, n));
          Debug("CRCc: ", pl, 2);
        }
      }
      delay(100);

      return n;
    }
    
    // Convert 16bit value to 2 * 8bit
    void set16bValue(byte cmd[], unsigned int v)
    {
      cmd[0] = (byte)((v >> 8) & 0xff);
      cmd[1] = (byte)((v) & 0xff);
    }
    
    // Convert 2 * 8bit to 16bit value
    unsigned int get16bValue(byte cmd[])
    {
      return (unsigned int)(MBbuffer[3] << 8) | (unsigned int)(MBbuffer[4]);
    }
    
    // Convert 4 * 8bit to 32bit value
    unsigned long get32bValue(byte cmd[])
    {
      return (unsigned long)(MBbuffer[3] << 8) | (unsigned long)(MBbuffer[4]) | (unsigned long)(MBbuffer[5] << 24) | (unsigned long)(MBbuffer[6] << 16);
    }

    // Modbus: Read input register (MB cmd 04)
    int MBReadInputRed(byte node, unsigned int regAdr, unsigned int noRegs, byte rsp[], int rsp_len)
    {
      byte MBcmd[MB_COMMAND_LENGTH];
      MBcmd[0] = node;
      MBcmd[1] = 4;                   // Modbus Read Input Register
      set16bValue(MBcmd + 2, regAdr);
      set16bValue(MBcmd + 4, noRegs);
      set16bValue(MBcmd + 2, regAdr);
      int crc = modbusCRC(MBcmd, 6);
      set16bValue(MBcmd + 6, crc);
    
      int n = serialCmd(MBcmd, MB_COMMAND_LENGTH, rsp, rsp_len);
      if (modbusCRCVerify(rsp, min(rsp_len, n))) return n;
      else return 0;
    }
    
    void loopDataRead()
    {
      MBbuffer_len = MBReadInputRed(1, 0, 1, MBbuffer, SERIAL_BUFFER_LENGTH);
      if (MBbuffer_len) Voltage = (int)(MBbuffer[3] << 8) | (int)(MBbuffer[4]);

      MBbuffer_len = MBReadInputRed(1, 1, 2, MBbuffer, SERIAL_BUFFER_LENGTH);
      if (MBbuffer_len) Current = get32bValue(MBbuffer+3);
      
      MBbuffer_len = MBReadInputRed(1, 3, 2, MBbuffer, SERIAL_BUFFER_LENGTH);
      if (MBbuffer_len) Power = get32bValue(MBbuffer+3);
      
      MBbuffer_len = MBReadInputRed(1, 5, 2, MBbuffer, SERIAL_BUFFER_LENGTH);
      if (MBbuffer_len) Energy = get32bValue(MBbuffer+3);
      
      MBbuffer_len = MBReadInputRed(1, 7, 1, MBbuffer, SERIAL_BUFFER_LENGTH);
      if (MBbuffer_len) Frequency = get16bValue(MBbuffer+3);

      MBbuffer_len = MBReadInputRed(1, 8, 1, MBbuffer, SERIAL_BUFFER_LENGTH);
      if (MBbuffer_len) PowerFactor = get16bValue(MBbuffer+3);

/*

          Alarm     = (int)(MBbuffer[21] << 8) | (int)(MBbuffer[22]);
          CRC       = (int)(MBbuffer[23] << 8) | (int)(MBbuffer[24]);
*/
          newDataAvailable = true;
      
    }

    void loopMqttPublish()
    {
      char pl[MQTT_PL_MAX_LEN];
    
//      if (newDataAvailable)
      {
          //String s;
          char fltstr[20];     
          //s = String((float)Voltage/10);
          
          dtostrf((float)Voltage/10, 12, 1, fltstr);
          sprintf(pl, "%sV", fltstr);
          mqttPublish("Measurements/U/Value", pl);
          
          dtostrf((float)Current/1000, 14, 3, fltstr);
          sprintf(pl, "%sA", fltstr);
          mqttPublish("Measurements/I/Value", pl);  
          
          dtostrf((float)Power/10, 12, 1, fltstr);
          sprintf(pl, "%sW", fltstr);
          mqttPublish("Measurements/P/Value", pl);
          
          dtostrf((float)Energy/1000, 14, 3, fltstr);
          sprintf(pl, "%skWh", fltstr);
          mqttPublish("Measurements/E/Value", pl);
          
          dtostrf((float)Frequency/10, 12, 1, fltstr);
          sprintf(pl, "%sHz", fltstr);
          mqttPublish("Measurements/F/Value", pl);
          
          dtostrf((float)PowerFactor/100, 13, 2, fltstr);
          sprintf(pl, "%s", fltstr);
          mqttPublish("Measurements/PF/Value", pl);

          newDataAvailable = false;
      }
   }
};

PZEM16_gw jmw(LED_BUILTIN_WEMOS_D1, false);




///////////////////////////////////////////////////////////////////////////
void setup() 
{
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  
  jmw.setup();
}

void loop() 
{
  jmw.loop();
}
