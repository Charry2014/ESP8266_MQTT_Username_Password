#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

/* 
 *  secrets.h defines things that are - secret :-)
 * The real secrets.h will not be in the repository
 * obviously, but you can see fake values that it 
 * defines below.
 * 
 */
#if 1
#include "secrets.h"

#else
#error Are you sure you want to use the fake stuff?

#define WIFI_SSID "my_wifi_ssid"
#define WIFI_PASSWORD "super_secret"

#define MQTT_USER  "username"
#define MQTT_PASSWORD "Lets_hack_it_777"

// Test Mosquitto server, various ports available
// 1883 is unencrypted, 8883 is encrypted no certificate required
// 8884 : MQTT, encrypted, client certificate required
// https://test.mosquitto.org/
#define MQTT_SERVER "test.mosquitto.org"
#define MQTT_PORT 1883

// Set x509 CA root (must match server cert) - this is taken from chain.pem on the broker
const char *CA_CHAIN_PEM PROGMEM = R"EOF("
-----BEGIN CERTIFICATE-----
fake stuff here... about 24 lines of ASCII characters ending in ==
-----END CERTIFICATE-----
")EOF";

// Fingerprint of the MQTT broker CA - for Letsencrypt certificates this is chain.pem I think
// openssl x509 -in  chain.pem -sha1 -noout -fingerprint
const char* fingerprint = "28:7C:0E:AC:E1:B9:00:9B:DB:4B:BA:19:0A:3D:4A:B5:F6:AA:4D:A6";

#endif




// LED configuration - write HIGH to turn off, LOW to turn on
#define led_built_in_ESP 2
#define led_built_in_Node 16

// MQTT Test Topics
#define in_topic "test/in"
#define out_topic "test"


#define SERIAL_BAUD_RATE 115200

#if MQTT_PORT == 1883
// Unsecured MQTT
WiFiClient espClient;

#else
BearSSL::WiFiClientSecure espClient;

#endif

PubSubClient client(MQTT_SERVER, MQTT_PORT, espClient);
String clientId = "ESP8266-";

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println();

  pinMode(led_built_in_ESP, OUTPUT);
  pinMode(led_built_in_Node, OUTPUT);
  digitalWrite(led_built_in_ESP, HIGH);
  digitalWrite(led_built_in_Node, HIGH);

  // Append a random string to the cliendId stub
  clientId += String(random(0xffff), HEX);
  Serial.print("setup: This device MQTT client ID is ");
  Serial.println(clientId);
  
  // Configure essential network things - connect the wifi
  // set the system time and setInsecure is the magic option
  // to be able to connect to PubSubClients with username/password
  setup_wifi(WIFI_SSID, WIFI_PASSWORD);
  setup_time();
  espClient.setInsecure();
 
  // Verify the server using the certificate fingerprint
  if( verifytls(MQTT_SERVER, MQTT_PORT) != 0)
    stop("Server identity error");
}


void loop() {
  if (!client.connected()) {
    reconnect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  }
  client.loop();
  
  // Publishes a randomised hello world message
  String message = String(random(0xffff), HEX) + " Hello World";
  Serial.print("loop: publishing message ");
  Serial.println(message);
  client.publish(out_topic, message.c_str());
  delay(1000);
}

void reconnect(const char * client_id, const char * username, const char * password) {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect(client_id, username, password)) {
      Serial.println("reconnect: connected using PubSubClient");
    } else {
      Serial.print("reconnect: failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      led_blink(led_built_in_ESP, 15);
    }
  }
}


void setup_wifi(const char * ssid, const char * pwd) {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.print("setup_wifi: Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pwd);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("setup_wifi: WiFi connected - IP address: ");
  Serial.println(WiFi.localIP());
}

int verifytls(const char * server, unsigned int port) {
  // Returns 0 for success or 1 for failure
  int retval = 1;
  
  // Use WiFiClientSecure class to create TLS connection
  Serial.print("verifytls: ");
  print_server_connect("WiFiClientSecure", server, port);
  if (espClient.connect(server, port) == 0)
    Serial.println("verifytls: WiFiClientSecure connection failed");
  else if (espClient.verify(fingerprint, server) == 0)
    Serial.println("verifytls: certificate doesn't match - stopping!");
  else
  {
    Serial.println("verifytls: WiFiClientSecure connected and certificate matches");
    retval = 0;
  }
  return retval;
}

void callback(char* topic, byte* payload, unsigned int length) {
 Serial.print("Message arrived [");
 Serial.print(topic);
 Serial.print("] ");
 for (int i = 0; i < length; i++) {
  char receivedChar = (char)payload[i];
  Serial.print(receivedChar);
 }
 Serial.println();
}



void setup_time(){
  // Synchronize time using SNTP. This is necessary to verify that
  // the TLS certificates offered by the server are currently valid.
  Serial.print("set_time: Setting time using SNTP ");
  configTime(8 * 3600, 0, "de.pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 1000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("set_time: Current time: ");
  Serial.print(asctime(&timeinfo));
  }

void print_server_connect(const char * details, const char * server, unsigned int port)
  {
  Serial.print("Attempting ");
  Serial.print(details);
  Serial.print(" connection to server ");
  Serial.print(server);
  Serial.print(" on port ");
  Serial.println(port);    
  }
 
void led_blink(unsigned int led, unsigned int count)
{
  int i = 0;
  for( ;i< count; i++)
  {
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
    delay(500);
  }
  digitalWrite(led, HIGH);
}

// The error handling graveyard - put the device into a deep sleep forever
// In practice this will wake every 71 minutes and then sleep again
void stop(const char * message)
{
  Serial.print("stop: ");
  Serial.println(message);
  while(1)
    ESP.deepSleep(UINT32_MAX);
}
