/**
 *  ESP8266 (NodeMCU) LED sectional based on the Arduino framework.
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */

#pragma once
#ifndef LED_SECTIONAL_H
#define LED_SECTIONAL_H

#define NUM_AIRPORTS          22      // Number of airports

#define DO_LIGHTNING                  // If defined, flash the airport LED when lightning is in the METAR
#ifdef DO_LIGHTNING
  #define LIGHTNING_INTERVAL  2       // How often the lightning animation will run, in seconds
#endif

#define WIND_THRESHOLD        25      // Wind/gust speed, above which, when VFR will change from green to yellow.  Set high to disable.

#define DO_SLEEP                      // If defined, automatically stop polling and turn off LEDs after a certain time
#ifdef DO_SLEEP
  //#define TIMEZONE            "America/Los_Angeles"  // Specify a time zone if the automatic method doesn't work.  For a list of valid timezones, see http://worldtimeapi.org/timezones
  #define SLEEP_WD_START      22      // The hour (local) at which the sectional will sleep on weekdays
  #define SLEEP_WD_END        17      // The hour (local) at which the sectional will wake up on weekdays
  
  #define SLEEP_WE_START      23      // The hour (local) at which the sectional will sleep on weekends
  #define SLEEP_WE_END        7       // The hour (local) at which the sectional will wake up on weekends
  
  const bool dayIsWeekend[] = {       // Specify which days of the week should use "weekend" hours (true) or "weekday" hours (false)
    true,   // Sunday
    false,  // Monday
    false,  // Tuesday
    false,  // Wednesday
    false,  // Thursday
    false,  // Friday
    true    // Saturday
  };
#endif

#define DO_TSL2561                    // If defined, use the TSL2561 illuminance sensor to dynamically set the LED brightness
#ifdef DO_TSL2561
  const unsigned luxMap[][2] = {      // Map of lux vs LED intensity.
    // Lux,   Intensity
    { 0,      0 },                    // REQUIRED index of 0
    { 1,      4 },                    // Fill in any points desired to define the curve.
    { 50,     4 },                    // Spaces between points are linearly interpreted.
    { 125,    8 },                    // Use SECTIONAL_DEBUG to print out sensor values to help tuning.
    { 300,    32 },                   // First and last index are required but the values can be changed.
    { 65536,  128 }                   // REQUIRED index of 65536
  };
#endif

#define LED_DATA_PIN          4
#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB
#define BRIGHTNESS_DEFAULT    32

const char ssid[] = "CHANGE-ME";      // your network SSID (name)
const char pass[] = "CHANGE-ME";      // your network password (use for WPA, or use as key for WEP)

// Test result includes airports that are not listed here as well
// as this list includes airports that don't exist in the result.
std::vector<String> airports({
  "KBOS", // 1
  "KCQX", // 2
  "KPVC", // 3
  "KPVD", // 4
  "KFMH", // 5
  "KPSF", // 6
  "LIFR", // 7
  "IFR",  // 8
  "MVFR", // 9
  "WVFR", // 10
  "VFR",  // 11
  "KXXX", // 12
  "KXXX", // 13
  "KXXX", // 14
  "KXXX", // 15
  "KXXX", // 16
  "KXXX", // 17
  "KXXX", // 18
  "KXXX", // 19
  "KXXX", // 20
  "KXXX", // 21
  "KXXX"  // 22
});

#define AW_SERVER                 "project802.net"
#define BASE_URI                  "/hosted/led_sectional_test_data?stationString="
#define METAR_REQUEST_INTERVAL_S  (30*1000)

#define SECTIONAL_DEBUG

#endif
