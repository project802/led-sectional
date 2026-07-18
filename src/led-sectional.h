/**
 *  ESP8266 (NodeMCU) LED sectional based on the Arduino framework.
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */

#pragma once
#ifndef LED_SECTIONAL_H
#define LED_SECTIONAL_H

#include <NeoPixelBus.h>
#include <map>

#define WIFI_SSID             "CHANGE-ME" // your network SSID (name)
#define WIFI_PASS             "CHANGE-ME" // your network password

#define AW_SERVER             "aviationweather.gov"
#define BASE_URI              "api/data/metar?format=geojson&taf=false&ids="

#define LIGHTNING_INTERVAL    5       // How often the lightning animation will run, in seconds.  0 to disable.

#define WIND_THRESHOLD        25      // Wind/gust speed, above which, when VFR will change from green to yellow.  Set high to disable.

#define TSL2561_ADDRESS       TSL2561_ADDR_FLOAT
#define BRIGHTNESS_DEFAULT    32      // Default brightness which optionally gets changed if DO_TSL2561 is enabled

const unsigned luxMap[][2] = {        // Map of lux vs LED intensity.
  // Lux,   Intensity
  { 0,      0 },                      // REQUIRED index of 0
  { 1,      4 },                      // Fill in any points desired to define the curve.
  { 50,     8 },                      // Spaces between points are linearly interpreted.
  { 125,    16 },                     // Use SECTIONAL_DEBUG to print out sensor values to help tuning.
  { 300,    32 },                     // First and last index are required but the values can be changed.
  { 65536,  128 }                     // REQUIRED index of 65536
};

struct AirportConditions
{
  bool     valid;
  unsigned attempts;
  unsigned pixel;
  String   flightCategory;
  bool     lightning;
  unsigned windSpeed;
  unsigned windGust;
};

std::map<String, AirportConditions> airports = {
  { "KJFK", { .pixel = 0 } },
  { "KHPN", { .pixel = 1 } },
  { "KPOU", { .pixel = 2 } },
  { "KALB", { .pixel = 3 } },
  { "KPSF", { .pixel = 4 } },
  { "KOXC", { .pixel = 5 } },
  { "KISP", { .pixel = 6 } },
  { "KJPX", { .pixel = 7 } },
  { "KSNC", { .pixel = 8 } },
  { "KBAF", { .pixel = 9 } },
  { "KEEN", { .pixel = 10 } },
  { "KORH", { .pixel = 11 } },
  { "KIJD", { .pixel = 12 } },
  { "KBID", { .pixel = 13 } },
  { "KPVD", { .pixel = 14 } },
  { "KBOS", { .pixel = 15 } },
  { "KMHT", { .pixel = 16 } },
  { "KPVC", { .pixel = 17 } },
  { "KCQX", { .pixel = 18 } },
  { "KACK", { .pixel = 19 } },
  { "KMVY", { .pixel = 20 } },
  { "KFMH", { .pixel = 21 } },
};

RgbColor orange ( 255, 128,   0 );
RgbColor magenta( 255,   0, 255 );
RgbColor purple ( 160,  32, 240 );
RgbColor red    ( 255,   0,   0 );
RgbColor blue   (   0,   0, 255 );
RgbColor green  (   0, 255,   0 );
RgbColor white  ( 255, 255, 255 );
RgbColor black  (   0,   0,   0 );
RgbColor yellow ( 255, 255,   0 );

std::map<String, RgbColor> flightCategoryColors = {
  { "LIFR", magenta },
  { "IFR",  red },
  { "MVFR", blue },
  { "VFR",  green },
  { "",     black }
};

#ifdef __has_include
  #if __has_include( "local_config.h" )
    #include "local_config.h"
  #endif
#endif

#endif
