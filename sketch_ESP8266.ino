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


const char compile_info[] = __FILE__ " - " __DATE__ " " __TIME__;

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
#define VERSION_STRING "1.0.0"

// Define this to disable all real MQTT input & output
#define TESTING_DISABLE_MQTT_OUTPUT

#define SERIAL_BAUD_RATE 115200


// GPIO configuration
#define STATUS_ERROR_LED       2     // led_built_in_ESP
#define GATE_OPEN_STATUS_LED  16     // Green LED
#define GATE_OPEN_SWITCH_GPIO  5
#define GATE_RING_STATUS_LED   4     // Red LED

#define DEBOUNCE_DELAY_MS     50

// MQTT Test Topics
#define in_topic "test/in"
#define out_topic "test"

// MQTT Gate Control Topic
#define MQTT_GATE_OPEN_TOPIC "gate"
#define MQTT_GATE_OPEN_PAYLOAD "0"

// For testing we use another MQTT topic
#if defined (TESTING_DISABLE_MQTT_OUTPUT)
#define MQTT_GATE_EVENT_TOPIC in_topic
#else
#define MQTT_GATE_EVENT_TOPIC "doorbell/ring"
#endif

// Configuration for the GPIO Pins - specify input or output, initial value.
typedef struct GPIO_Setup { int GPIO; int INOUT; int Initial; };
static const GPIO_Setup GPIO_list[] = {
  {STATUS_ERROR_LED,      OUTPUT, LOW},
  {GATE_OPEN_STATUS_LED,  OUTPUT, HIGH},
  {GATE_OPEN_SWITCH_GPIO, INPUT,  -1},
  {GATE_RING_STATUS_LED,  OUTPUT, HIGH},
  {-1, -1, -1}
  };


/**
 * Simple wrapper for ISR based LED blinking
 * 
 * Clock of 80MHz - TIM_DIV16  80 MHz /  16 =   5    MHz or 0.2 microseconds
 * So .0000002 * 5000 = .001 seconds or 1000 Hz.
 * 
 * Clock of 80MHz - TIM_DIV256 80 MHz / 256 = 312500 KHz or  31 microseconds
 * 
 * The Blink arrays give the on and off periods, in milliseconds. They must be
 * exactly two elements long.
 */
#define TIMER_MS_FACTOR      312
unsigned int Blink1[]          = { 500*TIMER_MS_FACTOR, 200*TIMER_MS_FACTOR };
unsigned int Blink_attention[] = { 200*TIMER_MS_FACTOR, 200*TIMER_MS_FACTOR };


typedef struct PatternStruct
  {
    unsigned int repeat;
    unsigned int * pattern;
    unsigned int GPIO;
    int activeLevel;
  };

volatile PatternStruct activePattern;




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
 * Enable the Watchdog, and set serial baud rate
 * Configure GPIO pins and attach interrupts
 * Connect the Wifi set the clock via NTP and 
 * finally verify the MQTT server identity
 */
void setup() {
  wdt_enable(WDTO_8S);
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println();

#if defined (TESTING_DISABLE_MQTT_OUTPUT)
  Serial.print("!DEBUG ENABLED! - ");
#endif
  Serial.print(VERSION_STRING " - Build details ");
  Serial.println(compile_info);
    
  // Set up the GPIOs
  for( const GPIO_Setup * gp = &GPIO_list[0]; gp->GPIO != -1; gp++) {
    pinMode(gp->GPIO, gp->INOUT);
    if(gp->INOUT == OUTPUT)
      digitalWrite(gp->GPIO, gp->Initial);
  }

  
  // Set GATE_OPEN_SWITCH_GPIO to be input with interrupt 
  attachInterrupt(digitalPinToInterrupt(GATE_OPEN_SWITCH_GPIO), ISR_open_gate_switch, FALLING);

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
    digitalWrite(GATE_OPEN_STATUS_LED, HIGH);
    digitalWrite(GATE_RING_STATUS_LED, HIGH);
    reconnect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
    digitalWrite(GATE_OPEN_STATUS_LED, LOW);
    digitalWrite(GATE_RING_STATUS_LED, LOW);

  }
  mqtt_client.loop();

  if(gate_switch_trigger_time != 0 && trigger_gate_release == true) {
    // The trigger time is set in the ISR and means the button was pressed
    // This triggers the release, then waits a second before carrying on
    trigger_gate_release = false;
    set_blink_pattern(GATE_OPEN_STATUS_LED, 10, Blink1);
#if defined (TESTING_DISABLE_MQTT_OUTPUT)
    Serial.println("DEBUG: Opening Gate disabled for testing");
#else
    Serial.println("Opening Gate - publishing to " MQTT_GATE_OPEN_TOPIC);
    mqtt_client.publish(MQTT_GATE_OPEN_TOPIC, MQTT_GATE_OPEN_PAYLOAD);
#endif
  }

  if( millis() > gate_switch_trigger_time + 1000 && 
      digitalRead(GATE_OPEN_SWITCH_GPIO) == HIGH) {
    // Retrigger the IRQ when the time has elapsed and the switch
    // has been released.
    gate_switch_trigger_time = 0;
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
  if(interrupt_time - debounce_time < DEBOUNCE_DELAY_MS)
    return;
  debounce_time = interrupt_time;

  if(gate_switch_trigger_time == 0) {
    gate_switch_trigger_time = interrupt_time;
    trigger_gate_release = true;
  }
}

/**
 * timer1 ISR - work through the blink pattern repeat count.
 */
void ICACHE_RAM_ATTR onTimerISR(){
  static volatile unsigned int idx = 0;

  activePattern.activeLevel = activePattern.activeLevel ? LOW : HIGH;
  digitalWrite(activePattern.GPIO, activePattern.activeLevel);
  if(activePattern.repeat > 0) {
    activePattern.repeat -= 1;
    timer1_write(activePattern.pattern[idx & 1]);
  }
  idx += 1;
}

/**
 * set_blink_pattern - Blink the GPIO with a pattern dictated by the pattern
 * and repeat a number of times.
 * 
 * Configure timer1 interrupt 
 */
void set_blink_pattern(unsigned int GPIO, unsigned int repeat, unsigned int * pattern)
{
  timer1_disable();
  
  activePattern.repeat = repeat * 2;
  activePattern.pattern = pattern;
  activePattern.GPIO = GPIO;
  activePattern.activeLevel = HIGH;
  
  timer1_attachInterrupt(onTimerISR);
  digitalWrite(GPIO, activePattern.activeLevel);

  timer1_isr_init( );
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_SINGLE);
  timer1_write(pattern[0]);
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
      set_blink_pattern(STATUS_ERROR_LED, 20, Blink_attention);
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
  if ( (char) *payload == '1')
    set_blink_pattern(GATE_RING_STATUS_LED, 10, Blink1);
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
