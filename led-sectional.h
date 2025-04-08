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

#define LIGHTNING_INTERVAL    5       // How often the lightning animation will run, in seconds.  0 to disable.

#define WIND_THRESHOLD        25      // Wind/gust speed, above which, when VFR will change from green to yellow.  Set high to disable.

//#define TIMEZONE            "America/Los_Angeles"  // Specify a time zone if the automatic method doesn't work.  For a list of valid timezones, see http://worldtimeapi.org/timezones
#define SLEEP_WD_START        22      // The hour (local) at which the sectional will sleep on weekdays
#define SLEEP_WD_END          17      // The hour (local) at which the sectional will wake up on weekdays

#define SLEEP_WE_START        23      // The hour (local) at which the sectional will sleep on weekends
#define SLEEP_WE_END          7       // The hour (local) at which the sectional will wake up on weekends

const bool dayIsWeekend[] = {         // Specify which days of the week should use "weekend" hours (true) or "weekday" hours (false)
  true,   // Sunday
  false,  // Monday
  false,  // Tuesday
  false,  // Wednesday
  false,  // Thursday
  false,  // Friday
  true    // Saturday
};

// List of US 2023 federal holidays as day of the year
std::vector<unsigned> holidays({
  2,    // NY
  16,   // MLK Jr Birthday
  51,   // Washington's Birthday
  149,  // Memorial Day
  185,  // Independence Day
  247,  // Labor Day
  282,  // Columbus Day
  314,  // Verterans Day
  327,  // Thanksgiving Day
  359   // Christmas Day
});

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


const char ssid[] = "CHANGE-ME";      // your network SSID (name)
const char pass[] = "CHANGE-ME";      // your network password (use for WPA, or use as key for WEP)
struct AirportConditions
{
  String  flightCategory;
  bool    lightning;
};

#define NUM_AIRPORTS          22

std::vector<std::pair<String, AirportConditions>> airports = {
  { "KJFK", {} },  // 1
  { "KHPN", {} },  // 2
  { "KPOU", {} },  // 3
  { "KALB", {} },  // 4
  { "KPSF", {} },  // 5
  { "KOXC", {} },  // 6
  { "KISP", {} },  // 7
  { "KHTO", {} },  // 8
  { "KSNC", {} },  // 9
  { "KBAF", {} },  // 10
  { "KEEN", {} },  // 11
  { "KORH", {} },  // 12
  { "KIJD", {} },  // 13
  { "KBID", {} },  // 14
  { "KPVD", {} },  // 15
  { "KBOS", {} },  // 16
  { "KMHT", {} },  // 17
  { "KPVC", {} },  // 18
  { "KCQX", {} },  // 19
  { "KACK", {} },  // 20
  { "KMVY", {} },  // 21
  { "KFMH", {} },  // 22
};

RgbColor orange ( 255, 128,   0 );
RgbColor magenta( 255,   0, 255 );
RgbColor purple ( 160,  32, 240 );
RgbColor red    ( 255,   0,   0 );
RgbColor blue   (   0,   0, 255 );
RgbColor green  (   0, 255,   0 );
RgbColor white  ( 255, 255, 255 );
RgbColor black  (   0,   0,   0 );

std::map<String, RgbColor> flightCategoryColors = {
  { "LIFR", magenta },
  { "IFR",  red },
  { "MVFR", blue },
  { "VFR",  green },
  { "",     white }
};

#endif