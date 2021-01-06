#if !defined (FAKE_SECRETS_H)
#define FAKE_SECRETS_H
#else
#error Multiple includes of fake_secrets.h
#endif

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

