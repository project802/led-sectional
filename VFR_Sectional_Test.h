#ifndef VFR_SECTIONAL_H
#define VFR_SECTIONAL_H

#define NUM_AIRPORTS          22      // Number of airports

#define DO_LIGHTNING                  // If defined, flash the airport LED when lightning is in the METAR
#ifdef DO_LIGHTNING
  #define LIGHTNING_INTERVAL  2       // How often the lightning animation will run, in seconds
#endif

#define WIND_THRESHOLD        25      // Wind/gust speed, above which, when VFR will change from green to yellow.  Set high to disable.

#define DO_SLEEP                      // If defined, automatically stop polling and turn off LEDs after a certain time
#ifdef DO_SLEEP
  #define SLEEP_WD_START_ZULU 3       // The hour (UTC+0) at which the sectional will sleep on weekdays
  #define SLEEP_WD_END_ZULU   22      // The hour (UTC+0) at which the sectional will wake up on weekdays
  
  #define SLEEP_WE_START_ZULU 3       // The hour (UTC+0) at which the sectional will sleep on weekends
  #define SLEEP_WE_END_ZULU   13      // The hour (UTC+0) at which the sectional will wake up on weekends
  
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
  const unsigned luxMap[][2] = {
    { 0, 0 },
    { 1, 4 },
    { 50, 4 },
    { 125, 8 },
    { 300, 32 },
    { 65536, 128 }
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
