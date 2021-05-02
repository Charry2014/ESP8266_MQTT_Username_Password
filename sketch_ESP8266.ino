/**
 * sketch_ESP8266 - use the ESP8266 to publish to a topic on a secure
 * Mosquitto MQTT broker. The broker is hosted on AWS free tier and has
 * valid certificates issued by LetsEncrypt. 
 * 
 * The intended function is to release our garden gate on the push of a button
 * by publishing to an MQTT topic.
 * 
 * This is running on a AZDelivery NodeMCU Lua Amica Module V2 ESP8266
 * It is based heavily on things found on the 'net and especially lots 
 * of useful guides on Random Nerd Tutorials such as this one:
 * https://randomnerdtutorials.com/esp8266-nodemcu-digital-inputs-outputs-arduino/
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
//#include <avr/wdt.h>

/* 
 * secrets.h defines things that are - secret :-)
 * The real secrets.h will not be in the repository
 * obviously, but you can see fake values that it 
 * defines below.
 * 
 */
#if 1
#include "secrets.h"

#else
// Delete this #error line if you really want to use the fake secrets
#error Are you sure you want to use the fake_secrets.h?
#include "fake_secrets.h"

#endif




// GPIO configuration
#define STATUS_ERROR_LED       2     // led_built_in_ESP
#define GATE_OPEN_STATUS_LED  16     // led_built_in_Node
#define GATE_OPEN_SWITCH_GPIO  5
#define GATE_RING_STATUS_LED   4

// MQTT Test Topics
#define in_topic "test/in"
#define out_topic "test"

// MQTT Gate Control Topic
#define MQTT_GATE_OPEN_TOPIC "gate"
#define MQTT_GATE_OPEN_PAYLOAD "0"

#define MQTT_GATE_EVENT_TOPIC in_topic // "doorbell/ring"

// Configuration for the GPIO Pins - specify input or output, initial value.
typedef struct GPIO_Setup { int GPIO; int INOUT; int Initial; };
static const GPIO_Setup GPIO_list[] = {
  {STATUS_ERROR_LED,      OUTPUT, LOW},
  {GATE_OPEN_STATUS_LED,  OUTPUT, LOW},
  {GATE_OPEN_SWITCH_GPIO, INPUT,  LOW},
  {GATE_RING_STATUS_LED,  OUTPUT, LOW},
  {-1, -1, -1}
  };

#define SERIAL_BAUD_RATE 115200

#if MQTT_PORT == 1883
// Unsecured MQTT - currently untested
WiFiClient espClient;

#else
BearSSL::WiFiClientSecure espClient;

#endif

PubSubClient mqtt_client(MQTT_SERVER, MQTT_PORT, espClient);
String clientId = "ESP8266-";

// Grubby globals - mostly for the ISR
volatile unsigned long gate_switch_trigger_time = 0;
volatile bool trigger_gate_release = false;

/**
 * Here we go - set everything up
 */
void setup() {
  ;

  wdt_enable(WDTO_8S);
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println();

  for( const GPIO_Setup * gp = &GPIO_list[0]; gp->GPIO != -1; gp++) {
    pinMode(gp->GPIO, gp->INOUT);
    digitalWrite(gp->GPIO, gp->Initial);
  }

#if 0
  pinMode(STATUS_ERROR_LED, OUTPUT);
  pinMode(GATE_OPEN_STATUS_LED, OUTPUT);
  //pinMode(GATE_RING_STATUS_LED, OUTPUT);
  digitalWrite(STATUS_ERROR_LED, LOW);
  digitalWrite(GATE_OPEN_STATUS_LED, LOW);
  //digitalWrite(GATE_RING_STATUS_LED, LOW);
  
  // Set GATE_OPEN_SWITCH_GPIO to be input with interrupt 
  pinMode(GATE_OPEN_SWITCH_GPIO, INPUT);
#endif
  attachInterrupt(digitalPinToInterrupt(GATE_OPEN_SWITCH_GPIO), ISR_open_gate_switch, RISING);

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


/**
 * Main loop, with no delay to keep it snappy, also no sleep yet
 */
void loop() {
  wdt_reset();
  if (!mqtt_client.connected()) {
    reconnect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  }
  mqtt_client.loop();

  if(gate_switch_trigger_time != 0 && trigger_gate_release == true) {
    // The trigger time is set in the ISR and means the button was pressed
    // This triggers the release, then waits a second before carrying on
    trigger_gate_release = false;
    Serial.println("Opening Gate");
    // mqtt_client.publish(MQTT_GATE_OPEN_TOPIC, MQTT_GATE_OPEN_PAYLOAD);
  }

  if( millis() > gate_switch_trigger_time + 1000 && 
      digitalRead(GATE_OPEN_SWITCH_GPIO) == LOW) {
    // Retrigger the IRQ when the time has elapsed and the switch
    // has been released.
    gate_switch_trigger_time = 0;
    digitalWrite(GATE_OPEN_STATUS_LED, LOW);
  }
}

/**
 * Interrupt handler for gate release switch
 * Record the time when the interrupt happens
 * and trigger the feedback LED.
 * 
 * Add code to debounce the switch by waiting 50ms 
 * between interrupts
 */
ICACHE_RAM_ATTR void ISR_open_gate_switch(void) {
  static unsigned long debounce_time = 0;
  unsigned long interrupt_time = millis();
  if(interrupt_time - debounce_time < 50)
    return;
  debounce_time = interrupt_time;

  if(gate_switch_trigger_time == 0) {
    gate_switch_trigger_time = interrupt_time;
    trigger_gate_release = true;
    digitalWrite(GATE_OPEN_STATUS_LED, HIGH);
  }
}

/**
 * Loop until we're reconnected to the MQTT broker
 * When the connection is up we subscribe to the 
 * MQTT topic MQTT_GATE_EVENT_TOPIC
 */
void reconnect(const char * client_id, const char * username, const char * password) {
  while (!mqtt_client.connected()) {
    // Attempt to connect
    if (mqtt_client.connect(client_id, username, password)) {
      Serial.println("reconnect: connected using PubSubClient");
      mqtt_client.subscribe(MQTT_GATE_EVENT_TOPIC);
      mqtt_client.setCallback(mqtt_callback);
    } else {
      Serial.print("reconnect: failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      led_blink(STATUS_ERROR_LED, 15);
    }
  }
}

/**
 * We start by connecting to a WiFi network
 * Has a whole lot of debug as it doesn't happen often
 */
void setup_wifi(const char * ssid, const char * pwd) {
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

/**
 * Verify the identity of a server using its fingerprint
 * Use WiFiClientSecure class to create TLS connection
 * Returns 0 for success or 1 for failure
 */
int verifytls(const char * server, unsigned int port) {
  int retval = 1;
  
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


/**
 * Synchronize time using SNTP. This is necessary to verify that
 * the TLS certificates offered by the server are currently valid.
 */
 void setup_time(){
  // 
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


/**
 * Handler for MQTT message received.
 * 
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    char receivedChar = (char)payload[i];
    Serial.print(receivedChar);
  }
  Serial.println();
  digitalWrite(4, HIGH);
}


/**
 * The error handling graveyard - put the device into a deep sleep forever
 * Used for fatal error handling to prevent discharging the battery
 * In practice this will wake every 71 minutes and then sleep again
 */
 void stop(const char * message)
{
  Serial.print("stop: ");
  Serial.println(message);
  while(1)
    ESP.deepSleep(UINT32_MAX);
}


/**
 * Below here is a lot of unnecessary junk that could be removed.
 */
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

#if 0
  // 8< -------- Cuttings
  // Publishes a randomised hello world message
  String message = String(random(0xffff), HEX) + " Hello World";
  Serial.print("loop: publishing message ");
  Serial.println(message);
  mqtt_client.publish(out_topic, message.c_str());

  if (digitalRead(5) == HIGH)
    digitalWrite(GATE_OPEN_STATUS_LED, HIGH);
  else
    digitalWrite(GATE_OPEN_STATUS_LED, LOW);
#endif  
