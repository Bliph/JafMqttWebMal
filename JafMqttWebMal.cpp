#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <PubSubClient.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "JafMqttWebMal.h"

/**

Enkel
1. Instasier JafMqttWeb
2. Kall setup()
3. Kall loop()

Override:
1. Lag ny klasse K som arver JafMqttWeb
2.a Override f.eks. handleRead
2.b Override f.eks. loopDataRead
2.c Override f.eks. loopMqttPublish
2.d Override f.eks. mqttCallback
3. Kall setup()
4. Kall loop()

*/



#ifndef __JafMqttWebMal_Instances__
#define __JafMqttWebMal_Instances__

// MQTT
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
PubSubClient tb_client(espClient);

IPAddress local_IP(192,168,0,1);
IPAddress gateway(10,0,0,1);
IPAddress subnet(255,255,255,0);
ESP8266WebServer web_server(80);
//bool OTAInProgress = false;
//bool debug = false;

JafMqttWeb* __JMW__;

void sHandleRoot()      { __JMW__->handleRoot(); }
void sHandleSetup()     { __JMW__->handleSetup(); }
void sHandleReadSys()   { __JMW__->handleReadSys(); }
void sHandleRead()      { __JMW__->handleRead(); }
void sHandleNotFound()  { __JMW__->handleNotFound(); }
void sMqttCallback(char* topic, byte* payload, unsigned int length) 
{ 
  __JMW__->mqttCallback(topic, payload, length); 
}

#endif __JafMqttWebMal_Instances__

JafMqttWeb::JafMqttWeb(int _led, bool _debug)
{
  __JMW__ = this;
  led = _led;
  debug = _debug;
}

// Converts a byte to hex-string (2 characters), same as String(x, HEX)?
String JafMqttWeb::byte2string(byte n)
{
  char a = (n & 0x0F);
  char b = (n & 0xF0) >> 4;
  a <= 9 ? a += '0' : a += 'A' - 0x0a; 
  b <= 9 ? b += '0' : b += 'A' - 0x0a; 

  return (String)b+(String)a;
}

// Converts hex-character to value. Legal characters: ['0'..'9', 'a'..'f', 'A'..'F']
byte JafMqttWeb::hex2val(char c)
{
  byte r = 0;
  if (c >= 'A' && c <= 'F') r = c - 'A' + 0x0a;
  else if (c >= 'a' && c <= 'f') r = c - 'a' + 0x0a;
  else if (c >= '0' && c <= '9') r = c - '0';

  return r;
}

String JafMqttWeb::bytes2string(byte* data, int len)
{
  String s = "";
  for (int i = 0; i < len; i++)
  {
    s = s + byte2string(data[i]); 
//    if (i < len-1) s = s + ' ';
  }
  return s;
}

// Serial write with debug output    
void JafMqttWeb::Debug(char* desc, byte* data, int len)
{
  char pl[20];
  if (debug) Serial.println(bytes2string(data, len));

  String topic = "Debug/" + String(desc) + "/Length";
  sprintf(pl, "%i", len);
  mqttPublish((char*)topic.c_str(), pl);
        
  topic = "Debug/" + String(desc) + "/Data";
  mqttPublishByteArray((char*)topic.c_str(), data, len);
}

void JafMqttWeb::Debug(char* desc, char* txt)
{
  String topic = "Debug/" + String(desc);
  mqttPublish((char*)topic.c_str(), txt);
}



// Write config to file
void JafMqttWeb::utilConfigWrite(String fname, String value)
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
bool JafMqttWeb::utilConfigRead(String fname, String &value)
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
void JafMqttWeb::wifi_reconnect()
{
  if (WiFi.status() != WL_CONNECTED) 
  {
    WiFi.reconnect();
    WiFi.waitForConnectResult();
  }
}


void JafMqttWeb::mqtt_reconnect()
{
  String mqtt_id_unique = mqtt_id + "_" + byte2string(mac[3])+byte2string(mac[4])+byte2string(mac[5]);
  if (!mqtt_client.connected()) 
  {
    if (debug) Serial.print("mqtt_client.connect(" + mqtt_id_unique + ", " +  mqtt_user + ", " + mqtt_password + ") = ");
    if (debug) Serial.println(mqtt_client.connect(mqtt_id_unique.c_str(), mqtt_user.c_str(), mqtt_password.c_str()));

    if (debug) Serial.println("Reconnectiong MQTT ID=" + mqtt_id_unique);
    mqtt_client.connect(mqtt_id_unique.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
  }
     
  if (!tb_client.connected()) 
  {
    if (debug) Serial.print("tb_client.connect(" + mqtt_id_unique + ", " +  tb_access_token + ", " + mqtt_password + ") = ");
    if (debug) Serial.println(tb_client.connect(mqtt_id_unique.c_str(), tb_access_token.c_str(), mqtt_password.c_str()));

    if (debug) Serial.println("Reconnectiong MQTT ID=" + mqtt_id_unique);
    tb_client.connect(mqtt_id_unique.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
String JafMqttWeb::debugDump(bool html)
{
  if (debug) Serial.println("debugDump()");
    
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
void JafMqttWeb::handleRoot() 
{
  if (debug) Serial.println("handleRoot()");

  digitalWrite(led, LOW);
  String message = "<!DOCTYPE html>\n";
  message += "<html><body>\n";
  message += "WIFI ESP8288 Template Gateway Web Server<br>\n(C) Johan &Aring;tland F&oslash;rsvoll 2019<br><br><br>\n\n\n";
  message += "<a href=""/"">/ (root)</a><br>\n";
  message += "<a href=""/read"">/read</a>&emsp;- Read some data<br>\n";
  message += "<a href=""/setup"">/setup</a>&emsp;- Setup Wifi, etc. etc.<br>\n";
  message += "<a href=""/readsys"">/readsys</a>&emsp;- Read system status<br>\n";
  message += "/readsys parameters:<br>\n"; 
  message += "/readsys?help=1<br>\n";
  message += "/readsys?debug=1<br>\n";
  message += "/readsys?header=1<br>\n";
  message += "/readsys?html=1<br>\n";
  message += "/readsys?refresh=1<br>\n";

  message += "</body></html>\n";

  web_server.send(200, "text/html", message);
  
  digitalWrite(led, HIGH);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void JafMqttWeb::handleSetup() 
{  
  if (debug) Serial.println("handleSetup()");
    
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
    if (web_server.argName(i) == "tb_access_token")   tb_access_token = String(web_server.arg(i));
    
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
    utilConfigWrite("/tb_access_token.cfg", tb_access_token);
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
  message += "    ThingsBoard Access Token: <input type=\"text\" name=\"tb_access_token\" value = \"" +String(tb_access_token)+ "\"><br>\n";
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
void JafMqttWeb::handleReadSys() 
{
  if (debug) Serial.println("handleReadSys()");
  
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

    message += newline + newline;
    message += debugDump(html);
    message += newline + newline;
  }


  if (html) message += "  </body>\n</html>";
  
  if (html) web_server.send(200, "text/html", message);
  else web_server.send(200, "text/plain", message);

//  digitalWrite(led, HIGH);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void JafMqttWeb::handleRead() 
{
  if (debug) Serial.println("handleRead()");
    
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
  message += "\read\n";
/**
 * Legg inn mer data som skal legges på WEB HER!!!
 * 
 * 
 */
 
  web_server.send(200, "text/plain", message);

//  digitalWrite(led, HIGH);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void JafMqttWeb::handleNotFound()
{
  if (debug) Serial.println("handleNotFound()");

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
void JafMqttWeb::loopDataRead_scheduler(unsigned long runtime_millis)
{
  if (millis() > current_millis_read + runtime_millis)
  {
    loopDataRead();
    current_millis_read = millis();
  }
}

void JafMqttWeb::loopDataRead()
{
/**
 * Les data fra serieport eller gjør noe annet smart HER
 * 
 * 
 */
}

void JafMqttWeb::mqttPublish(char* _topic, char* pl)
{
  char topic[MQTT_TOPIC_MAX_LEN];
  sprintf(topic, "/%s%s", mqtt_topic_prefix.c_str(), _topic);
  
  mqtt_client.publish(topic, pl);
  if (debug) 
  {
    Serial.print(topic);
    Serial.print(" = ");
    Serial.println(pl);
  }
}

void JafMqttWeb::tbPublishTelemetry(char* pl)
{
  tb_client.publish(tb_telemtry_topic.c_str(), pl);
  
  if (debug) 
  {
    Serial.print(tb_telemtry_topic);
    Serial.print(" = ");
    Serial.println(pl);
  }

}

void JafMqttWeb::tbPublishAttributes(char* pl)
{
  tb_client.publish(tb_attribute_topic.c_str(), pl);
  
  if (debug) 
  {
    Serial.print(tb_attribute_topic);
    Serial.print(" = ");
    Serial.println(pl);
  }
}

void JafMqttWeb::tbPublishAttribute(char* key, char* value)
{
  char pl[MQTT_PL_MAX_LEN];
  sprintf(pl, "{\"%s\":\"%s\"}", key, value);
  tbPublishAttributes(pl);
}

void JafMqttWeb::mqttPublishByteArray(char* _topic, byte* data, int len)
{
  char pl[MQTT_PL_MAX_LEN];
  if (len > 0)
  {
    String s = bytes2string(data, len);
    strncpy(pl, s.c_str(), sizeof(pl)-1);
    mqttPublish(_topic, pl);
  }
}

void JafMqttWeb::loopMqttPublish_scheduler(unsigned long runtime_millis)
{
  char pl[MQTT_PL_MAX_LEN];
  
  if (millis() > current_millis_publish + runtime_millis)
  {
    loopMqttPublish();

    current_millis_publish = millis();
  }
}

void JafMqttWeb::loopMqttPublish_status(unsigned long runtime_millis)
{
  char pl[MQTT_PL_MAX_LEN];
  
  if (millis() > current_millis_status_publish + runtime_millis)
  {
      String s;
      StaticJsonDocument<MQTT_PL_MAX_LEN> tbs;
      time_t now = time(nullptr);

      s = String((uint32_t)now);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqttPublish("Status/Time/Value", pl);
      tbs["time"] = pl;

      s = WiFi.localIP().toString();
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqttPublish("Status/IP/Value", pl);
      tbs["IP"] = pl;

      strncpy(pl, ssid.c_str(), sizeof(pl)-1);
      mqttPublish("Status/SSID/Value", pl);
      tbs["SSID"] = pl;

      strncpy(pl, ap_ssid_unique.c_str(), sizeof(pl)-1);
      mqttPublish("Status/AP_SSID/Value", pl);
      tbs["AP_SSID"] = pl;

      s = bytes2string(mac, 6);
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqttPublish("Status/MAC/Value", pl);
      tbs["MAC"] = pl;

      s = String(WiFi.RSSI());
      strncpy(pl, s.c_str(), sizeof(pl)-1);
      mqttPublish("Status/RSSI/Value", pl);
      tbs["RSSI"] = pl;

      if (debug) 
      {
        mqttPublish("Status/Debug/Enabled", "On");
        tbs["Debug"] = "On";
      }
      else 
      {
        mqttPublish("Status/Debug/Enabled", "Off");
        tbs["Debug"] = "Off";
      }

      serializeJson(tbs, pl);
      tbPublishAttributes(pl);
      current_millis_status_publish = millis();
  }
}

void JafMqttWeb::loopMqttPublish()
{
/*
 * Legg inn flere publish her
 * 
 * "C:\Program Files (x86)\mosquitto\mosquitto_sub" -v -h 10.0.0.102 -t "/geiterasen/#"
 */
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// MQTT Callback
void JafMqttWeb::mqttCallback(char* topic_, byte* payload_, unsigned int length)
{
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Extremely simple task scheduler. 
// Loop runs for <runtime_millis> ms and ends. Index is preserved and continues the next call
void JafMqttWeb::blink()
{
  if (((blipblop) && (millis() > current_millis_blink + led_off)) || ((!blipblop) && (millis() > current_millis_blink + led_on)))
  {
    blipblop = !blipblop;
    current_millis_blink = millis();
    digitalWrite(led, blipblop);
    if (debug) Serial.print('.');
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 

void JafMqttWeb::loop(void)
{ 
  mqtt_interleave = ((mqtt_interleave + 1) % 1);

  ArduinoOTA.handle();
  if (!OTAInProgress) 
  {
    if (WiFi.status() == WL_CONNECTED) 
    {
      led_on = LED_OK_1;
      led_off = LED_OK_0;
      mqtt_reconnect(); 
      loopDataRead_scheduler(1000);
      loopMqttPublish_scheduler(1000);
      loopMqttPublish_status(10000);

      last_reconnect = millis();
    }
    else 
    {
// Serial.println("Blop!");
      led_on = LED_BAD_1;
      led_off = LED_BAD_0;
      if (debug) Serial.println("ssid     = "+ssid+"\n");
      if (debug) Serial.println("password = "+password+"\n");
    
      // Reboot if connection lost for more than 300.000ms = 300s = 5min
      if ((millis() > last_reconnect + 10000) && (mqtt_interleave == 0)) if (debug) Serial.println("> 10 sek siden connection");
      if (millis() > last_reconnect + 300000) ESP.restart();
    }
  }

  if (mqtt_interleave == 0) mqtt_client.loop();
  if (mqtt_interleave == 0) tb_client.loop();

  web_server.handleClient();
  blink();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void JafMqttWeb::setup(void)
{
  led_on = LED_BAD_1;
  led_off = LED_BAD_0;

  WiFi.macAddress(mac);
  ap_ssid_unique = ap_ssid+"_"+byte2string(mac[3])+byte2string(mac[4])+byte2string(mac[5]);

  pinMode(led, OUTPUT);
//  digitalWrite(led, HIGH);

  Serial.begin(9600, SERIAL_8N1);
//  Serial.swap();
  
  Serial.setTimeout(500);

  if (debug) 
  {
    Serial.println("");
    Serial.println("\nAP SSID Unique=" + ap_ssid_unique);
  }
  
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
    if (debug) Serial.println("WiFi.reconnect()");
    delay(5000);
    
//    blipblop = !blipblop;
//    digitalWrite(led, blipblop);    
  }
  
  if (debug) 
  {
//    IPAddress ip = WiFi.remoteIP();
//    Serial.println("Remote IP Address: " + ip);
    IPAddress ip = WiFi.localIP();
    Serial.print("Local IP Address: ");
    Serial.println(ip);
    long rssi = WiFi.RSSI();
    Serial.print("Signal Strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
  }
  
  OTAInProgress = false;

  configTime(timezone * 3600, dst * 0, "pool.ntp.org", "time.nist.gov");
  MDNS.begin(ap_ssid_unique.c_str());

  ArduinoOTA.onStart([]() 
  {
    __JMW__->OTAInProgress = true;
    SPIFFS.end();
    if (__JMW__->debug) Serial.println("OTA Start");
  });
  
  ArduinoOTA.onEnd([]() 
  {
    __JMW__->OTAInProgress = false;
    if (__JMW__->debug) Serial.println("\nOTA End\n");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
  {
    if (__JMW__->debug) Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    __JMW__->blipblop = !(__JMW__->blipblop);
    digitalWrite(__JMW__->led, __JMW__->blipblop);
  });
  
  ArduinoOTA.onError([](ota_error_t error) 
  {
    if (__JMW__->debug) 
    {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  
  web_server.on("/", sHandleRoot);
  web_server.on("/read", sHandleRead);
  web_server.on("/readsys", sHandleReadSys);
  web_server.on("/setup", sHandleSetup);

  web_server.onNotFound(sHandleNotFound);
  web_server.begin();
  
  // MQTT
  mqtt_client.setServer(mqtt_server.c_str(), mqtt_port.toInt());
  tb_client.setServer(mqtt_server.c_str(), mqtt_port.toInt());
  mqtt_client.setCallback(sMqttCallback);
}
