# led-sectional

[![Codacy Badge](https://app.codacy.com/project/badge/Grade/6d49a017b89b4e7395385a1821d93631)](https://app.codacy.com/gh/project802/led-sectional/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)

## 2025 Refactor
* Use latest aviationweather.gov API
* Upgrade to esp8266 board support version 3.1.2
* FastLED broke in esp8266 3.x so swap it out for NeoPixelBus.
* Removed tinyXML now that the API gives us Json
* Remove manual HTTP request management in favor of HTTPClient and ChunkedDecodingStream for interoperability

## Introduction
Display flight conditions on a sectional chart using LEDs.  This project is based on the Arduino framework using an ESP8266 NodeMCU.  Originally based on code from [WKHarmon](https://github.com/WKHarmon/led-sectional) and expanded with some additional features.

### METARs
Data is downloaded from the United States' National Weather Service using the Data API on aviationweather.gov (https://aviationweather.gov/data/api/) and displayed on a series of individually addressable LEDs (WS2812B).

### Dynamic LED brightness
Using a TSL2561 light sensor from Adafruit, dynamically control the brightness of the LEDs based on the ambient light.  This improves legibility and conserves power across high and low-lux situations.  For example, turn the LEDs off when the room is dark and scale them up to full brightness when under direct sunlight.

### Go To Sleep, Weekend and Holiday Behavior
The World Time API is used to automatically set the time so the sectional can go to sleep and wake up at pre-programmed times for either weekdays or weekends.  When sleeping, the LEDs are turned off and METAR queries pause.  This is to be good citizens to the Data API and conserve power.  US federal holidays are treated as weekends.

## Get Started
* Download the Arduino IDE
* Add support for the ESP8266 board (add the URL to the board manager and install version 3.1.2)
* Add the following libraries:
  * ArduinoJson (7.3.1)
  * NeoPixelBus (2.8.3)
  * Adafruit Unified Sensor (1.1.15)
  * Adafruit TS2561 (1.1.2)
  * StreamUtils (1.9.0)
* Open the .ino project in this repository.
* Set your Wi-Fi credentials in the led_sectional.h file
* Upload the project 

If you have a successful upload, you can get started with your customizations and hardware installation.

## NodeMCU Wiring Configuration
* WS2812 
  * +5V to a supply such as the VIN pin
  * Data pin (DIN) to D4 (GPI02)
  * GND to GND

* TSL2561 
  * VIN to +3.3V
  * GND to GND
  * SDA to D2
  * SCL to D1

## Wishlist
I'd like to not use insecure SSL, but the certificate for aviationweather.gov rotates multiple times a year and that will be annoying to keep up with.

An API that tells if the day is a holiday would be nice. Maybe I'll host one if I can find a way to get the dates automatically.

Use WiFiManager to set the Wi-Fi credentials and do connection management instead of manually managing the state.

Have simple serial port commands to stimulate various functionality for debug and testing.
