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


// D0 = GPIO16 = PIN16
// D4 = GPIO02 = PIN02
#define D0 16
#define D1 5 // I2C Bus SCL (clock)
#define D2 4 // I2C Bus SDA (data)
#define D3 0
#define D4 2 // Same as "LED_BUILTIN", but inverted logic
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO 
#define D7 13 // SPI Bus MOSI
#define D8 15 // SPI Bus SS (CS)
#define D9 3 // RX0 (Serial console)
#define D10 1 // TX0 (Serial console)

#define SERIAL_BUFFER_LENGTH 400

#define min(a, b) (((a) <= (b)) ? (a) : (b))
#define max(a, b) (((a) >= (b)) ? (a) : (b))

#define RSP_LEN_MAX 50
#define MQTT_PL_MAX_LEN 256
#define MQTT_TOPIC_MAX_LEN 1024

class MAL_gw : public JafMqttWeb
{
  public:
    MAL_gw(int led = LED_BUILTIN_WEMOS_D1, bool debug = false) : JafMqttWeb(led, debug) { };
};

MAL_gw jmw(LED_BUILTIN_WEMOS_D1, true);

///////////////////////////////////////////////////////////////////////////
void setup() 
{
  jmw.setup();
}

void loop() 
{
  jmw.loop();
}

