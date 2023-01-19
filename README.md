# led-sectional
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/6d49a017b89b4e7395385a1821d93631)](https://www.codacy.com/gh/project802/led-sectional/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=project802/led-sectional&amp;utm_campaign=Badge_Grade)
[![Build Status](https://travis-ci.com/project802/led-sectional.svg?branch=master)](https://travis-ci.com/project802/led-sectional)

## Introduction
Display flight conditions on a sectional chart using LEDs.  This project is based on the Arduino framework using an ESP8266 NodeMCU.  Originally based on code from [WKHarmon](https://github.com/WKHarmon/led-sectional) and expanded with some additional features.

### METARs
Data is downloaded from the United States' National Weather Service using the TDS API on aviationweather.gov and displayed on a series of individually addressable LEDs (WS2812B).

### Dynamic LED brightness
Using a TSL2561 light sensor from Adafruit, dynamically control the brightness of the LEDs based on the ambient light.  This improves legibility and conserves power across high and low-lux situations.  For example, turn the LEDs off when the room is dark and scale them up to full brightness when under direct sunlight.

### Go To Sleep, Weekend and Holiday Behavior
The World Time API is used to automatically set the time so the sectional can go to sleep and wake up at pre-programmed times for either weekdays or weekends.  When sleeping, the LEDs are turned off and METAR queries pause.  This is to be good citizens to the TDS API and conserve power.  US federal holidays are treated as weekends.

## Get Started
* Download the Arduino IDE
* Add support for the ESP8266 board
* Add the "ArduinoJson", "FastLED", "Adafruit Unified Sensor" and "Adafruit TS2561" libraries
* Open the .ino project in this repository.
* Upload the project 

If you have a successful upload, you can get started with your customizations and hardware installation.

## NodeMCU Wiring Configuration
* WS2812 
  * +5V to a supply such as the VIN pin
  * Data pin (DIN) to D4
  * GND to GND
  
* TSL2561 
  * VIN to +3.3V
  * GND to GND
  * SDA to D2
  * SCL to D1
