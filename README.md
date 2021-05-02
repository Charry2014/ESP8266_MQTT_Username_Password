# ESP8266 MQTT With Username & Password Authentication

## Introduction

A small project for using the snappily titled AZ_Delivery [NodeMCU Lua Amica Module V2 ESP8266 ESP-12F WIFI Wifi Development Board](https://www.az-delivery.de/en/products/nodemcu#description) with CP2102 with username & password secured MQTT. The board is connected to the computer over USB where it receives its power and I am creating in in Arduino Studio and debugging it on the Mac in CoolTerm. Eventually this will move to Visual Studio Code. This project has been developed and tested using macOS Big Sur, but should be fine on everything else.

## Requirements

The requirements  of the project are for kind-of secure communication of MQTT messages published from a small, low power board, that can run for some days on a small battery like a couple of AA (we will come to the power saving aspect of this later). This will be used in a larger home automation project where many platforms - including the Raspberry Pi and Android and iPhones - are already playing their part. 

## Description

Most of the code in the Arduino sketch is boiler plate stuff you can find all over the internet but the one thing I have put together here that took me a while to get going is the authentication using a username and a password. The magic line that got this going is to add the following before attemtpting to connect to the broker.

```c++
espClient.setInsecure();
```

My thanks to GitHub user [brnyza](https://github.com/brnyza) for his tip which you can find in this [thread](https://github.com/esp8266/Arduino/issues/4826).

The server certificates are checked - the MQTT broker I use does have valid certificates that are provided by Letsencrypt - but otherwise I do not use them for authenticating the communication. This may may come later when all the other clients in the can support it.

## Change History

**02.05.21** - I tidied up the GPIO initialisation with a little wrapper to make sure that all the GPIOs are correctly initialised, and I added the first step of the Watchdog implementation. The code now subscribes to an MQTT topic but doesn't really do anything with it.  

**28.04.21** - Added a basic ISR to handle the button press. This isn't strictly necessary but is me learning more about Arduino programming and the hardware it is based on. This version fulfills all the basic requirements of my project and I will proceed to build a fully enclosed but USB powered (no batteries) version that we can actually start using.

## Known Issues

1. The interrupt triggers not just on rising edges, but also apparently on falling. This may be caused by the switch bouncing and triggering false edges. I will add an RC network to the hardware to do hardware debouncing of this. For now the software debouncing works well enough.

## Some Notes

1. There is some attempt in the sketch to use unsecured MQTT if the MQTT port is 1883 - this is currently untested but might work.
2. ~~The receiving of published messages isn't tested, because well I only need to originate messages here~~
3. Use #if 0 in the first lines of code to get the fake secrets active and fill them in with sensible values, if you want to. Obviously my own secrets.h is not in the repository.
4. Full certificate based authentication seems nicely described [here](https://gist.github.com/eLement87/133cddc5bd0472daf5cb35a20bfd811e)
5. And an issue from the debugging stage to [knolleary](https://github.com/knolleary/pubsubclient/issues/806)

