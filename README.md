# ESP8266_MQTT

## Introduction

A small project for using the snappily titled AZ_Delivery [NodeMCU Lua Amica Module V2 ESP8266 ESP-12F WIFI Wifi Development Board](https://www.az-delivery.de/en/products/nodemcu#description) with CP2102 with username & password secured MQTT. The board is connected to the computer over USB where it receives its power and I am creating in in Arduino Studio and debugging it on the Mac in CoolTerm. Eventually this will move to Visual Studio Code.

## Requirements

The requirements  of the project are for kind-of secure communication of MQTT messages published from a small, low power board, that can run for some days on a small battery like a couple of AA (we will come to the power saving aspect of this later). This will be used in a larger home automation project where many platforms - including the Raspberry Pi and Android and iPhones - are already playing their part. At the moment this device does not need to subscribe to any topics.

## Description

Most of the code in the Arduino sketch is boiler plate stuff you can find all over the internet but the one thing I have put together here that took me a while to get going is the authentication using a username and a password. The magic line that got this going is to add the following before attemtpting to connect to the broker.

```c++
espClient.setInsecure();
```

My thanks to GitHub user [brnyza](https://github.com/brnyza) for his tip which you can find in this [thread](https://github.com/esp8266/Arduino/issues/4826).

The server certificates are checked - the MQTT broker I use does have valid certificates that are provided by Letsencrypt - but otherwise I do not use them for authenticating the communication. This may may come later when all the other clients in the can support it.

## Some Notes

1. There is some attempt in the sketch to use unsecured MQTT if the MQTT port is 1883 - this is currently untested but might work.
2. The receiving of published messages isn't tested, because well I only need to originate messages here
3. Use #if 0 in the first lines of code to get the fake secrets active and fill them in with sensible values, if you want to. Obviously my own secrets.h is not in the repository.
4. Full certificate based authentication seems nicely described [here](https://gist.github.com/eLement87/133cddc5bd0472daf5cb35a20bfd811e)
5. And an issue from the debugging stage to [knolleary](https://github.com/knolleary/pubsubclient/issues/806)

