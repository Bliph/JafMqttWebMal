#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <PubSubClient.h>
#include <FS.h>
#include "JafMqttWebMal.h"
//#include <SoftwareSerial.h>



// D0 = GPIO16 = PIN16
// D4 = GPIO02 = PIN02

#define LED_BUILTIN 4
#define SERIAL_BUFFER_LENGTH 400

#define min(a, b) (((a) <= (b)) ? (a) : (b))
#define max(a, b) (((a) >= (b)) ? (a) : (b))

#define RSP_LEN_MAX 50
#define MQTT_PL_MAX_LEN 256
#define MQTT_TOPIC_MAX_LEN 1024

class HAN_gw : public JafMqttWeb
{
  public:
    HAN_gw(int led = LED_BUILTIN_NODEMCU, bool debug = false) : JafMqttWeb(led, debug) { };

  private:
    bool newDataAvailable = false;
    
    byte HANbuffer[SERIAL_BUFFER_LENGTH];
    int HANbuffer_len = 0;

    float P_pos, P_neg;
    float Q_pos, Q_neg;
    float I1, I2, I3;
    float U1, U2, U3;
    float A_pos, A_neg;
    float R_pos, R_neg;

    byte OBIS_P_pos[6] = {1,1,1,7,0,255};
    byte OBIS_P_neg[6] = {1,1,2,7,0,255};
    byte OBIS_Q_pos[6] = {1,1,3,7,0,255};
    byte OBIS_Q_neg[6] = {1,1,4,7,0,255};
    byte OBIS_I1[6] = {1,1,31,7,0,255};
    byte OBIS_I2[6] = {1,1,51,7,0,255};
    byte OBIS_I3[6] = {1,1,71,7,0,255};
    byte OBIS_U1[6] = {1,1,32,7,0,255};
    byte OBIS_U2[6] = {1,1,52,7,0,255};
    byte OBIS_U3[6] = {1,1,72,7,0,255};
    byte OBIS_A_pos[6] = {1,1,1,8,0,255};
    byte OBIS_A_neg[6] = {1,1,2,8,0,255};
    byte OBIS_R_pos[6] = {1,1,3,8,0,255};
    byte OBIS_R_neg[6] = {1,1,4,8,0,255};
    String hourlyTimeStamp = "";

  float getPower(byte* buffer)
  {
    return (float)(((long)(buffer[0]) << 24) | ((long)(buffer[1]) << 16) | ((long)(buffer[2]) << 8) | ((long)(buffer[3])));
  }

  float getCurrent(byte* buffer)
  {
    return (float)(((long)(buffer[0]) << 24) | ((long)(buffer[1]) << 16) | ((long)(buffer[2]) << 8) | ((long)(buffer[3])))/100.0;
  }
  float getVoltage(byte* buffer)
  {
    return (float)(((int)(buffer[0]) << 8) | ((long)(buffer[1])));
  }

  float getEnergy(byte* buffer)
  {
    // Returns kWh/kvarh
    return getPower(buffer)/100.0;
  }

  int findOBIS(byte* buf, int len, byte* obis)
  {
    int n = -1;
    for (int i = 0; (i < len - 6) && (n < 0); i++)
    {
      if ((buf[i] == obis[0]) && 
        (buf[i+1] == obis[1]) &&
        (buf[i+2] == obis[2]) &&
        (buf[i+3] == obis[3]) &&
        (buf[i+4] == obis[4]) &&
        (buf[i+5] == obis[5])) n = i;
    }
    return n;
  }

  void extractOBIS()
  {
    int last_index = min(SERIAL_BUFFER_LENGTH, HANbuffer_len);
    int idx;
    idx = findOBIS(HANbuffer, last_index, OBIS_P_pos); 
    if (idx >= 0) P_pos = getPower(HANbuffer + idx + 7);
    idx = findOBIS(HANbuffer, last_index, OBIS_P_neg); 
    if (idx >= 0) P_neg = getPower(HANbuffer + idx + 7);
      
    idx = findOBIS(HANbuffer, last_index, OBIS_Q_pos); 
    if (idx >= 0) Q_pos = getPower(HANbuffer + idx + 7);
    idx = findOBIS(HANbuffer, last_index, OBIS_Q_neg); 
    if (idx >= 0) Q_neg = getPower(HANbuffer + idx + 7);
    
    idx = findOBIS(HANbuffer, last_index, OBIS_I1); 
    if (idx >= 0) I1 = getCurrent(HANbuffer + idx + 7);  
    idx = findOBIS(HANbuffer, last_index, OBIS_I2); 
    if (idx >= 0) I2 = getCurrent(HANbuffer + idx + 7);  
    idx = findOBIS(HANbuffer, last_index, OBIS_I3); 
    if (idx >= 0) I3 = getCurrent(HANbuffer + idx + 7);
    
    idx = findOBIS(HANbuffer, last_index, OBIS_U1); 
    if (idx >= 0) U1 = getVoltage(HANbuffer + idx + 7);    
    idx = findOBIS(HANbuffer, last_index, OBIS_U2); 
    if (idx >= 0) U2 = getVoltage(HANbuffer + idx + 7);    
    idx = findOBIS(HANbuffer, last_index, OBIS_U3); 
    if (idx >= 0) U3 = getVoltage(HANbuffer + idx + 7);  

    idx = findOBIS(HANbuffer, last_index, OBIS_A_pos); 
    if (idx >= 0) A_pos = getEnergy(HANbuffer + idx + 7);
    idx = findOBIS(HANbuffer, last_index, OBIS_A_neg); 
    if (idx >= 0) A_neg = getEnergy(HANbuffer + idx + 7);

    idx = findOBIS(HANbuffer, last_index, OBIS_R_pos); 
    if (idx >= 0) R_pos = getEnergy(HANbuffer + idx + 7);
    idx = findOBIS(HANbuffer, last_index, OBIS_R_neg); 
    if (idx >= 0) R_neg = getEnergy(HANbuffer + idx + 7);
  }

  

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // 
  void handleRoot() 
  {
  //  digitalWrite(led, LOW);
    web_server.send(200, "text/plain", "WIFI HAN Gateway Web Server (C) Johan &Aring;tland F&oslash;rsvoll 2018");
  //  digitalWrite(led, HIGH);
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // 
  void handleRead() 
  {
  //  digitalWrite(led, LOW);
    String message = "Read\n\n";
    message += "URI: ";
    message += web_server.uri();
    message += "\nMethod: ";
    message += (web_server.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += web_server.args();
    message += "\n";

    for (uint8_t i=0; i<web_server.args(); i++)
    {
      message += " " + web_server.argName(i) + ": " + web_server.arg(i) + "\n";
    }

    time_t now = time(nullptr);
    message += "Time time_t: " + (String)(uint32_t)now + "\n";
    message += "Length: " + String(HANbuffer_len) + "\n";
    int date = ((((int)(HANbuffer[17]) << 8) | (int)(HANbuffer[18])));
    message += "Date: " + String(date)+"."+String(HANbuffer[19])+"."+String(HANbuffer[20]) + "\n";
    message += "Time: " + String(HANbuffer[22])+":"+String(HANbuffer[23])+":"+String(HANbuffer[24]) + "\n";
    message += "Hourly: " + hourlyTimeStamp + "\n\n";
  //  extractOBIS();
    message += "P_pos: " + String(P_pos) + " W\n";
    message += "P_neg: " + String(P_neg) + " W\n";
    message += "Q_pos: " + String(Q_pos) + " var\n";
    message += "Q_neg: " + String(Q_neg) + " var\n";
    message += "I1: " + String(I1) + " A\n";
    message += "I2: " + String(I2) + " A\n";
    message += "I3: " + String(I3) + " A\n";
    message += "U1: " + String(U1) + " V\n";
    message += "U2: " + String(U2) + " V\n";
    message += "U3: " + String(U3) + " V\n";
    message += "A_pos: " + String(A_pos) + " kWh\n";
    message += "A_neg: " + String(A_neg) + " kWh\n";
    message += "R_pos: " + String(R_pos) + " kvarh\n";
    message += "R_neg: " + String(R_neg) + " kvarh\n";
    message += "\nData:\n";

    for (int i = 0; i < min(HANbuffer_len, SERIAL_BUFFER_LENGTH); i++)
    {
      if ((i == 17) || (i == 22) || (i == 26) || (i == 31) || (i == 47) || (i == 73) || 
      (i == 101) || (i == 114) || (i == 127) || (i == 140) || (i == 153) || (i == 166) || (i == 179) || (i == 192) || (i == 203) || (i == 214) ||
      (i == 225))
      {
  /*
        if (i == 22) 
        {
          int date = ((((int)(HANbuffer[17]) << 8) | (int)(HANbuffer[18])));
          message += " Date: " + String(date)+"."+String(HANbuffer[19])+"."+String(HANbuffer[20]);
        }
        if (i == 26) 
        {
          message += "   Time: " + String(HANbuffer[22])+":"+String(HANbuffer[23])+":"+String(HANbuffer[24]);
        }
        */
        message += "\n";
      }

  //    message += String(HANbuffer[i], HEX);
      message += byte2string(HANbuffer[i]);
    } 

    web_server.send(200, "text/plain", message);

  //  digitalWrite(led, HIGH);
  }


  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // 
  // Data read loop. Called from scheduler
  void loopDataRead()
  {
      if (Serial.available()) 
      {
        HANbuffer_len = Serial.readBytes(HANbuffer, SERIAL_BUFFER_LENGTH);
        if (HANbuffer_len >= 228) newDataAvailable = true;
      }
      
      if (HANbuffer_len > 0)  
      {
        if (HANbuffer_len > 100) 
        {
          extractOBIS();
          if (HANbuffer_len > 228)
          {
            int date = ((((int)(HANbuffer[17]) << 8) | (int)(HANbuffer[18])));
            hourlyTimeStamp = String(HANbuffer_len) + " " + String(date)+"."+String(HANbuffer[19])+"."+String(HANbuffer[20]) + " " + String(HANbuffer[22])+":"+String(HANbuffer[23])+":"+String(HANbuffer[24]);
          }
        }
      }
  /*
      int char_len = 0;

      if (len > 0)
      {
        newDataAvailable = true;
        memset(newDataString, '/0', sizeof(newDataString));
        char_len = sprintf(newDataString, "LEN: %i\n", len);
        
        for (int i = 0; i < min(min(len, SERIAL_BUFFER_LENGTH), 100); i++)
        {
          if ((i == 17) || (i == 22) || (i == 26) || (i == 31) || (i == 47) || (i = 55) || (i == 72) || (i == 80) || (i == 220))
          {
                char_len += sprintf(newDataString + char_len, "\n");
          }
              
          char_len += sprintf(newDataString + char_len, "%02X", (unsigned char)buffer[i]);
              
              
  //            newDataString = newDataString + "\n";
  //        newDataString = newDataString + byte2string(buffer[i]);
        } 
      }
  */    
  }

  void loopMqttPublish()
  {
    char pl[MQTT_PL_MAX_LEN];
    char topic[MQTT_TOPIC_MAX_LEN];

    if (newDataAvailable)
    {
        newDataAvailable = false;
        String s;

        s = String(P_pos);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/P_pos/Value", pl);
        
        s = String(P_neg);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/P_neg/Value", pl);
        
        s = String(Q_pos);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/Q_pos/Value", pl);
        
        s = String(Q_neg);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/Q_neg/Value", pl);

        s = String(I1);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/I1/Value", pl);

        s = String(I2);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/I2/Value", pl);

        s = String(I3);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/I3/Value", pl);

        s = String(U1);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/U1/Value", pl);

        s = String(U2);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/U2/Value", pl);

        s = String(U3);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/U3/Value", pl);

        s = String(A_pos);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/A_pos/Value", pl);
        
        s = String(A_neg);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/A_neg/Value", pl);
        
        s = String(R_pos);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/R_pos/Value", pl);
        
        s = String(R_neg);
        strncpy(pl, s.c_str(), sizeof(pl)-1);
        mqttPublish("MainMeter/R_neg/Value", pl);
    }
  }
};

HAN_gw jmw(LED_BUILTIN_WEMOS_D1, false);

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

