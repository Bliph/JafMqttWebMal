#ifndef __JafMqttWebMal_H__
#define __JafMqttWebMal_H__

#define LED_BUILTIN_NODEMCU   2
#define LED_BUILTIN_WEMOS_D1  4

#define LED_OK_1  100;
#define LED_OK_0  900;
#define LED_BAD_1 100;
#define LED_BAD_0 100;
    
#define SERIAL_BUFFER_LENGTH 400

#define min(a, b) (((a) <= (b)) ? (a) : (b))
#define max(a, b) (((a) >= (b)) ? (a) : (b))

#define RSP_LEN_MAX 50
#define MQTT_PL_MAX_LEN 1024
#define MQTT_TOPIC_MAX_LEN 1024

ESP8266WebServer web_server;

class JafMqttWeb
{
  public:
    JafMqttWeb(int led = LED_BUILTIN_NODEMCU, bool debug = false);

    // Converts a byte to hex-string (2 characters), same as String(x, HEX)?
    static String byte2string(byte n);

    // Converts hex-character to value. Legal characters: ['0'..'9', 'a'..'f', 'A'..'F']
    static byte hex2val(char c);

    // Converts a byte array to hex-string
    static String bytes2string(byte* data, int len);

    virtual String debugDump(bool html = false);
    virtual void handleRoot();
    virtual void handleSetup();
    virtual void handleReadSys();
    virtual void handleRead();
    virtual void handleNotFound();
    virtual void loopDataRead();
    virtual void loopMqttPublish();
    virtual void mqttCallback(char* topic_, byte* payload_, unsigned int length);
    virtual void mqttPublish(char* topic, char* pl);
    virtual void mqttPublishByteArray(char* _topic, byte* data, int len);
    virtual void Debug(char* desc, byte* data, int len);
    virtual void Debug(char* desc, char* txt);
    
    virtual void loop(void);
    virtual void setup(void);
    
  private:
    // Write config to file
    virtual void utilConfigWrite(String fname, String value);

    // Read config from file
    virtual bool utilConfigRead(String fname, String &value);

    virtual void wifi_reconnect();
    virtual void mqtt_reconnect();

    virtual void blink();
    virtual void loopDataRead_scheduler(unsigned long runtime_millis);
    virtual void loopMqttPublish_scheduler(unsigned long runtime_millis);
    virtual void loopMqttPublish_status(unsigned long runtime_millis);

    byte mac[6];

    String ssid               = "Internkontrollsystem";
    String password           = "calvinandhobbes";
    String ap_ssid            = "MAL_gw";
    String ap_ssid_unique     = "_generated_";
    String ap_password        = "calvinandhobbes";

    String mqtt_topic_prefix  = "geiterasen/MAL_gw/";
    String mqtt_server        = "10.0.0.102";
    String mqtt_id            = "MAL_gw";
    String mqtt_user          = "dlnqnpps";
    String mqtt_password      = "cZT3WjehIE_8";

    String mqtt_port          = "1883";

    int timezone = 0;
    int dst = 0;

    unsigned long current_millis_status_publish = 0;
    unsigned long current_millis_publish = 0;
    unsigned long current_millis_read = 0;
    unsigned long current_millis_blink = 0;
    unsigned long last_reconnect = 0;

    int mqtt_interleave = 0;             
    int led         = LED_BUILTIN_NODEMCU;
    bool blipblop   = false;
    int led_on      = LED_BAD_1;
    int led_off     = LED_BAD_0;
    bool OTAInProgress = false;
    bool debug      = false;


};

#endif __JafMqttWebMal_H__
