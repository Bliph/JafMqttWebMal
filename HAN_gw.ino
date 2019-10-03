#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <PubSubClient.h>
#include <FS.h>
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

byte mac[6];
  
String ssid               = "Honningkrukka";
String password           = "calvinandhobbes";
String ap_ssid            = "HAN_gw";
String ap_password        = "calvinandhobbes";

String mqtt_server        = "10.0.0.102";
String mqtt_id            = "HAN_gw";
String mqtt_user          = "dlnqnpps";
String mqtt_password      = "cZT3WjehIE_8";
String mqtt_topic_prefix  = "geiterasen/";
String mqtt_port          = "1883";

IPAddress local_IP(192,168,0,1);
IPAddress gateway(10,0,0,1);
IPAddress subnet(255,255,255,0);
ESP8266WebServer web_server(80);
//SoftwareSerial swSeria1(2, 4, false, 256);

const int   led = LED_BUILTIN;
int timezone = 0;
int dst = 0;
bool OTAInProgress = false;
unsigned long current_millis = 0;
unsigned long current_millis_read = 0;
unsigned long last_reconnect = 0;
int loopIndex = -1;
bool blipblop = false;

bool newDataAvailable = false;
//char newDataString[1024] = "";
byte HANbuffer[SERIAL_BUFFER_LENGTH];
int HANbuffer_len = 0;

float P_pos, P_neg;
float Q_pos, Q_neg;
float I1, I2, I3;
float U1, U2, U3;
float A_pos, A_neg;
float R_pos, R_neg;

byte OBIS_P_pos[] = {1,1,1,7,0,255};
byte OBIS_P_neg[] = {1,1,2,7,0,255};
byte OBIS_Q_pos[] = {1,1,3,7,0,255};
byte OBIS_Q_neg[] = {1,1,4,7,0,255};
byte OBIS_I1[] = {1,1,31,7,0,255};
byte OBIS_I2[] = {1,1,51,7,0,255};
byte OBIS_I3[] = {1,1,71,7,0,255};
byte OBIS_U1[] = {1,1,32,7,0,255};
byte OBIS_U2[] = {1,1,52,7,0,255};
byte OBIS_U3[] = {1,1,72,7,0,255};
byte OBIS_A_pos[] = {1,1,1,8,0,255};
byte OBIS_A_neg[] = {1,1,2,8,0,255};
byte OBIS_R_pos[] = {1,1,3,8,0,255};
byte OBIS_R_neg[] = {1,1,4,8,0,255};
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

// MQTT
WiFiClient espClient;
PubSubClient mqtt_client(espClient);


// Converts a byte to hex-string (2 characters), same as String(x, HEX)?
String byte2string(byte n)
{
  char a = (n & 0x0F);
  char b = (n & 0xF0) >> 4;
  a <= 9 ? a += '0' : a += 'A' - 0x0a; 
  b <= 9 ? b += '0' : b += 'A' - 0x0a; 

  return (String)b+(String)a;
}

// Converts hex-character to value. Legal characters: ['0'..'9', 'a'..'f', 'A'..'F']
byte hex2val(char c)
{
  byte r = 0;
  if (c >= 'A' && c <= 'F') r = c - 'A' + 0x0a;
  else if (c >= 'a' && c <= 'f') r = c - 'a' + 0x0a;
  else if (c >= '0' && c <= '9') r = c - '0';

  return r;
}

// Write config to file
void utilConfigWrite(String fname, String value)
{
  if (SPIFFS.begin())
  {
    File f = SPIFFS.open(fname, "w+");
    if (f) 
    {
      f.print(value);
      f.close();
    }
  } 
}

// Read config from file
bool utilConfigRead(String fname, String &value)
{
  bool ret = false;
  if (SPIFFS.begin())
  {
    File f = SPIFFS.open(fname, "r");
    if (f) 
    {
      value = f.readString();
      f.close();
      ret = true;
    }
  }
  return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void wifi_reconnect()
{
  if (WiFi.status() != WL_CONNECTED) 
  {
    WiFi.reconnect();
    WiFi.waitForConnectResult();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
int mqtt_interleave = 0;

void mqtt_reconnect()
{
  String mqtt_id_unique = mqtt_id + "_" + byte2string(mac[3])+byte2string(mac[4])+byte2string(mac[5]);
  if (!mqtt_client.connected()) 
  {

    Serial.print("mqtt_client.connect(" + mqtt_id_unique + "," +  mqtt_user + ", " + mqtt_password + ") = ");
    Serial.println(mqtt_client.connect(mqtt_id_unique.c_str(), mqtt_user.c_str(), mqtt_password.c_str()));

    Serial.println("Reconnectiong MQTT ID=" + mqtt_id_unique);
    mqtt_client.connect(mqtt_id_unique.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
  }
     
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
String debugDump(bool html = false)
{
  String message = "URI: ";
  String newline;
  if (html) newline = "<br>\n";
  else newline = "\n";
  
  message += web_server.uri() + newline;
  message += "Method: ";
  message += ((web_server.method() == HTTP_GET) ? "GET" : "POST") + newline;
  message += "Arguments: ";
  message += web_server.args() + newline;

  for (uint8_t i = 0; i < web_server.args(); i++)
  {
    message += " " + web_server.argName(i) + ": " + web_server.arg(i) + newline;
  }

  message += "Free SRAM: 0"; // + freeRam();
  return message;
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
void handleSetup() 
{  
  String message = "";
  bool help, updatemqtt, updatessid;
  
  for (int i = 0; i < web_server.args(); i++)
  {
    if (web_server.argName(i) == "help")              help =        (web_server.arg(i) == "0") ? false : true;    
    if (web_server.argName(i) == "updatemqtt")        updatemqtt =  (web_server.arg(i) == "1") ? true : false;    
    if (web_server.argName(i) == "updatessid")        updatessid =  (web_server.arg(i) == "1") ? true : false;    

    if (web_server.argName(i) == "ssid")              ssid        = String(web_server.arg(i));
    if (web_server.argName(i) == "password")          password    = String(web_server.arg(i));
    if (web_server.argName(i) == "mqtt_server")       mqtt_server = String(web_server.arg(i));      
    if (web_server.argName(i) == "mqtt_port")         mqtt_port   = String(web_server.arg(i));        
    if (web_server.argName(i) == "mqtt_id")           mqtt_id     = String(web_server.arg(i));          
    if (web_server.argName(i) == "mqtt_user")         mqtt_user   = String(web_server.arg(i));        
    if (web_server.argName(i) == "mqtt_password")     mqtt_password = String(web_server.arg(i));    
    if (web_server.argName(i) == "mqtt_topic_prefix") mqtt_topic_prefix = String(web_server.arg(i));
    
    if (web_server.argName(i) == "reboot")    ESP.restart();    
    if ((web_server.argName(i) == "format") && (web_server.arg(i) == "yes")) SPIFFS.format();
  }

  if (updatemqtt)
  {
    utilConfigWrite("/mqtt_server.cfg", mqtt_server);
    utilConfigWrite("/mqtt_port.cfg", mqtt_port);
    utilConfigWrite("/mqtt_id.cfg", mqtt_id);
    utilConfigWrite("/mqtt_user.cfg", mqtt_user);
    utilConfigWrite("/mqtt_password.cfg", mqtt_password);
    utilConfigWrite("/mqtt_topic_prefix.cfg", mqtt_topic_prefix);
  }

  if (updatessid)
  {
    utilConfigWrite("/ssid.cfg", ssid);
    utilConfigWrite("/password.cfg", password);
  }
  
  int wifistatus = WiFi.status();
  String wifistatusStr = "";

  switch (wifistatus)
  {
    case WL_CONNECTED: wifistatusStr = "Connected"; break;
    case WL_CONNECT_FAILED: wifistatusStr = "Connect failed"; break;
    case WL_CONNECTION_LOST: wifistatusStr = "Connection lost"; break;
    case WL_DISCONNECTED: wifistatusStr = "Disconnected"; break;
    default: wifistatusStr = "Other (" + String(wifistatus) + ")";
  }

  message += "<!DOCTYPE html>\n";
  message += "<html><body>\n";
  message += "Current SSID: " + ssid +                "<br>\n";
  message += "Status: " + wifistatusStr +             "<br><br><br>\n";
  
  message += "  <form action=\"/setup\">  \n";
  message += "    MQTT Server:  <input type=\"text\" name=\"mqtt_server\" value = \"" +String(mqtt_server)+ "\"><br>\n";
  message += "    MQTT Port: <input type=\"number\" name=\"mqtt_port\" value = \"" +String(mqtt_port)+ "\"><br>\n";
  message += "    MQTT ID: <input type=\"text\" name=\"mqtt_id\" value = \"" +String(mqtt_id)+ "\"><br>\n";
  message += "    MQTT User: <input type=\"text\" name=\"mqtt_user\" value = \"" +String(mqtt_user)+ "\"><br>\n";
  message += "    MQTT Password: <input type=\"text\" name=\"mqtt_password\" value = \"" +String(mqtt_password)+ "\"><br>\n";
  message += "    MQTT Topic Prefix: <input type=\"text\" name=\"mqtt_topic_prefix\" value = \"" +String(mqtt_topic_prefix)+ "\"><br>\n";
  message += "    <input type=\"hidden\" name=\"updatemqtt\" value=\"1\">\n";
  message += "    <input type=\"submit\" value=\"Set\"><br>\n";
  message += "  </form><br><br>\n";
  
  message += "  <form action=\"/setup\">\n";
  message += "    SSID:\n";
  message += "    <select id=\"ssid\" name=\"ssid\">\n";
  int numSsid = WiFi.scanNetworks();
  for (int i = 0; i < numSsid; i++) 
  {
    String ssid_scan = WiFi.SSID(i);
    message += "<option value=\"" + ssid_scan + "\"";
    if (ssid_scan == ssid) message += " selected";
    message += ">" + ssid_scan + "</option>\n";
  }
  message += "    </select>\n";
  message += "    Password:  \n";
  message += "    <input type=\"text\" name=\"password\">\n";
  message += "    <input type=\"submit\" value=\"Set\">\n";
  message += "  </form><br><br>\n";
  
  message += "  <form action=\"/setup\" method=\"post\">\n";
  message += "    <input type=\"hidden\" name=\"help\"><br>\n";
  message += "    <input type=\"submit\" value=\"Help\"><br>\n";
  message += "  </form><br>\n";
    
  message += "  <form action=\"/setup\" method=\"post\">\n";
  message += "    <input type=\"hidden\" name=\"refresh\"><br>\n";
  message += "    <input type=\"submit\" value=\"Refresh\"><br>\n";
  message += "  </form><br>\n";
  
  message += "  <form action=\"/setup\" method=\"post\">  \n";
  message += "    Click here to activate new settings: <input type=\"hidden\" name=\"reboot\">\n";
  message += "    <input type=\"submit\" value=\"Reboot\"><br>\n";
  message += "  </form><br>\n";
  
  message += "  <form action=\"/setup\" method=\"post\">  \n";
  message += "    Click here for factory reset: <input type=\"hidden\" name=\"format\" value=\"yes\">\n";
  message += "    <input type=\"submit\" value=\"Format\"><br>\n";
  message += "  </form><br>\n";
  message += "</body></html>\n";

  web_server.send(200, "text/html", message);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void handleReadSys() 
{
  bool debug = false;
  bool var = false; 
  bool header = true;
  bool help = false;
  bool refresh = false;
  bool html = false;
  unsigned long refreshInterval = 0;
  String varName;
  String newline = "\n";
  String message = "";

//  digitalWrite(led, LOW);
  for (int i = 0; i < web_server.args(); i++)
  {
    if (web_server.argName(i) == "help")    help =   (web_server.arg(i) == "0") ? false : true;    
    if (web_server.argName(i) == "debug")   debug =  (web_server.arg(i) == "0") ? false : true;    
    if (web_server.argName(i) == "header")  header = (web_server.arg(i) == "0") ? false : true;   
    if (web_server.argName(i) == "html")    html =   (web_server.arg(i) == "0") ? false : true;   
    if (web_server.argName(i) == "refresh")  
    {
      refresh = (web_server.arg(i) == "0") ? false : true; 
      if (refresh) 
      {
        refreshInterval = web_server.arg(i).toInt();
        html = true;
      }
    }
    if (web_server.argName(i) == "var") { varName = web_server.arg(i); var = true; }
  }

  if (html) newline = "<br>\n";
  if (html) message += "<html>\n  <head>\n";
  if (refresh) message += "    <meta http-equiv=\"refresh\" content=\"" + (String)refreshInterval + "\">\n";
  if (html) message += "  </head>\n  <body>\n";

  if (help)
  {
    message = "Help:" + newline + newline;
    message += "readsys?help=0&debug=0&header=1&var=<somevar>&html=0&refresh=0"+newline+newline;
  }

  if (debug)
  {
    message += "URI: ";
    message += web_server.uri() + newline;
    message += "Method: ";
    message += (web_server.method() == HTTP_GET)?"GET":"POST" + newline;
    message += "Arguments: ";
    message += web_server.args() + newline;
  
    for (uint8_t i=0; i<web_server.args(); i++)
    {
      message += " " + web_server.argName(i) + ": " + web_server.arg(i) + newline;
    }

    message +=  newline + newline;
  }


  if (html) message += "  </body>\n</html>";
  
  if (html) web_server.send(200, "text/html", message);
  else web_server.send(200, "text/plain", message);

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
void handleNotFound()
{
//  digitalWrite(led, LOW);
  String message = "File Not Found\n\n";
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
  web_server.send(404, "text/plain", message);
//  digitalWrite(led, HIGH);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Extremely simple task scheduler. 
// Loop runs for <runtime_millis> ms and ends. Index is preserved and continues the next call
void loopDataRead(unsigned long runtime_millis)
{

  
  if (millis() > current_millis_read + runtime_millis)
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
    current_millis_read = millis();
  }
}

void loopDataPublish(unsigned long runtime_millis)
{
  char pl[MQTT_PL_MAX_LEN];
  char topic[MQTT_TOPIC_MAX_LEN];

  if (newDataAvailable)
  {
      newDataAvailable = false;
      String s;

      sprintf(topic, "/%sMainMeter/P_pos/Value", mqtt_topic_prefix.c_str());
      s = String(P_pos);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);
      
      sprintf(topic, "/%sMainMeter/P_neg/Value", mqtt_topic_prefix.c_str());
      s = String(P_neg);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);
      
      sprintf(topic, "/%sMainMeter/Q_pos/Value", mqtt_topic_prefix.c_str());
      s = String(Q_pos);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);
      
      sprintf(topic, "/%sMainMeter/Q_neg/Value", mqtt_topic_prefix.c_str());
      s = String(Q_neg);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);

      sprintf(topic, "/%sMainMeter/I1/Value", mqtt_topic_prefix.c_str());
      s = String(I1);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);

      sprintf(topic, "/%sMainMeter/I2/Value", mqtt_topic_prefix.c_str());
      s = String(I2);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);

      sprintf(topic, "/%sMainMeter/I3/Value", mqtt_topic_prefix.c_str());
      s = String(I3);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);

      sprintf(topic, "/%sMainMeter/U1/Value", mqtt_topic_prefix.c_str());
      s = String(U1);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);

      sprintf(topic, "/%sMainMeter/U2/Value", mqtt_topic_prefix.c_str());
      s = String(U2);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);

      sprintf(topic, "/%sMainMeter/U3/Value", mqtt_topic_prefix.c_str());
      s = String(U3);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);

      sprintf(topic, "/%sMainMeter/A_pos/Value", mqtt_topic_prefix.c_str());
      s = String(A_pos);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);
      
      sprintf(topic, "/%sMainMeter/A_neg/Value", mqtt_topic_prefix.c_str());
      s = String(A_neg);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);
      
      sprintf(topic, "/%sMainMeter/R_pos/Value", mqtt_topic_prefix.c_str());
      s = String(R_pos);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);
      
      sprintf(topic, "/%sMainMeter/R_neg/Value", mqtt_topic_prefix.c_str());
      s = String(R_neg);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqtt_client.publish(topic, pl);
      current_millis = millis();
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// MQTT Callback
void mqtt_callback(char* topic_, byte* payload_, unsigned int length)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 

void loop(void)
{ 
  mqtt_interleave = ((mqtt_interleave + 1) % 1);

  ArduinoOTA.handle();
  if (!OTAInProgress) 
  {
    if (WiFi.status() == WL_CONNECTED) 
    {
      mqtt_reconnect(); 
      loopDataRead(100);
      loopDataPublish(1000);

      last_reconnect = millis();
    }
    else 
    {
// Serial.println("Blop!");
      blipblop = ((millis() & 128) > 0);
//      digitalWrite(led, blipblop);
    Serial.println("ssid     = "+ssid+"\n");
    Serial.println("password = "+password+"\n");
    
      // Reboot if connection lost for more than 300.000ms = 300s = 5min
      if ((millis() > last_reconnect + 10000) && (mqtt_interleave == 0)) Serial.println("> 10 sek siden connection");
      if (millis() > last_reconnect + 300000) ESP.restart();
    }
  }

  if (mqtt_interleave == 0) mqtt_client.loop();

  web_server.handleClient();
 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void setup(void)
{
  WiFi.macAddress(mac);
  String ap_ssid_unique = ap_ssid+"_"+byte2string(mac[3])+byte2string(mac[4])+byte2string(mac[5]);

  
//  pinMode(led, OUTPUT);
//  digitalWrite(led, HIGH);

  Serial.begin(2400, SERIAL_8N1);
  Serial.swap();
//  swSeria1.begin(2400);
  
  Serial.setTimeout(500);

  Serial.println("AP SSID Unique=" + ap_ssid_unique);
  
  if (SPIFFS.begin())
  {
    utilConfigRead("/mqtt_server.cfg", mqtt_server);
    utilConfigRead("/mqtt_port.cfg", mqtt_port);
    utilConfigRead("/mqtt_id.cfg", mqtt_id);
    utilConfigRead("/mqtt_user.cfg", mqtt_user);
    utilConfigRead("/mqtt_password.cfg", mqtt_password);
    utilConfigRead("/mqtt_topic_prefix.cfg", mqtt_topic_prefix);

    utilConfigRead("/ssid.cfg", ssid);
    utilConfigRead("/password.cfg", password);    
  }

  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid_unique.c_str(), ap_password.c_str());
  
  WiFi.begin(ssid.c_str(), password.c_str());

  // Wait for connection
  for (int i = 0; (i < 3) && (WiFi.status() != WL_CONNECTED); i++)
  {
    WiFi.reconnect();
// Serial.println("WiFi.reconnect()");
    delay(5000);
    
    blipblop = !blipblop;
//    digitalWrite(led, blipblop);    
  }
  
  OTAInProgress = false;

  configTime(timezone * 3600, dst * 0, "pool.ntp.org", "time.nist.gov");
  MDNS.begin(ap_ssid_unique.c_str());

  ArduinoOTA.onStart([]() 
  {
    OTAInProgress = true;
    SPIFFS.end();
//    Serial.println("Start");
  });
  
  ArduinoOTA.onEnd([]() 
  {
    OTAInProgress = false;
//    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
  {
//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) 
  {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  
  web_server.on("/", handleRoot);
  web_server.on("/read", handleRead);
  web_server.on("/readsys", handleReadSys);
  web_server.on("/setup", handleSetup);

  web_server.onNotFound(handleNotFound);
  web_server.begin();
  
  // MQTT
  mqtt_client.setServer(mqtt_server.c_str(), mqtt_port.toInt());
  mqtt_client.setCallback(mqtt_callback);

}

