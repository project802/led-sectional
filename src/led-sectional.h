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

#define AIRPORT_DECL(iaco, pixelNo) { iaco, { .valid = false, .attempts = 0, .pixel = pixelNo, .flightCategory = "", .lightning = false, .windSpeed = 0, .windGust = 0 } }

std::map<String, AirportConditions> airports = {
  AIRPORT_DECL( "KJFK", 0 ),
  AIRPORT_DECL( "KHPN", 1 ),
  AIRPORT_DECL( "KPOU", 2 ),
  AIRPORT_DECL( "KALB", 3 ),
  AIRPORT_DECL( "KPSF", 4 ),
  AIRPORT_DECL( "KOXC", 5 ),
  AIRPORT_DECL( "KISP", 6 ),
  AIRPORT_DECL( "KJPX", 7 ),
  AIRPORT_DECL( "KSNC", 8 ),
  AIRPORT_DECL( "KBAF", 9 ),
  AIRPORT_DECL( "KEEN", 10 ),
  AIRPORT_DECL( "KORH", 11 ),
  AIRPORT_DECL( "KIJD", 12 ),
  AIRPORT_DECL( "KBID", 13 ),
  AIRPORT_DECL( "KPVD", 14 ),
  AIRPORT_DECL( "KBOS", 15 ),
  AIRPORT_DECL( "KMHT", 16 ),
  AIRPORT_DECL( "KPVC", 17 ),
  AIRPORT_DECL( "KCQX", 18 ),
  AIRPORT_DECL( "KACK", 19 ),
  AIRPORT_DECL( "KMVY", 20 ),
  AIRPORT_DECL( "KFMH", 21 )
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
