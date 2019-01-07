#ifndef VFR_SECTIONAL_H
#define VFR_SECTIONAL_H

#define NUM_AIRPORTS          22      // Number of airports

#define DO_LIGHTNING                  // If defined, flash the airport LED when lightning is in the METAR
#ifdef DO_LIGHTNING
  #define LIGHTNING_INTERVAL  5       // How often the lightning animation will run, in seconds
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
  #define LUX_THRESHOLD       300     // Threshold in lux for switching between high and low brightness
  #define BRIGHTNESS_LOW      8       // Brightness when below the low level
  #define BRIGHTNESS_HIGH     32      // Brightness when below the high level
#endif

#define LED_DATA_PIN          4
#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB
#define BRIGHTNESS_DEFAULT    8       // Default brightness which optionally gets changed if DO_TSL2561 is enabled

const char ssid[] = "CHANGE-ME";      // your network SSID (name)
const char pass[] = "CHANGE-ME";      // your network password (use for WPA, or use as key for WEP)

std::vector<String> airports({
  "KJFK", // 1
  "KHPN", // 2
  "KPOU", // 3
  "KALB", // 4
  "KPSF", // 5
  "KOXC", // 6
  "KISP", // 7
  "KHTO", // 8
  "KSNC", // 9
  "KBAF", // 10
  "KEEN", // 11
  "KORH", // 12
  "KIJD", // 13
  "KBID", // 14
  "KPVD", // 15
  "KBOS", // 16
  "KMHT", // 17
  "KPVC", // 18
  "KCQX", // 19
  "KACK", // 20
  "KMVY", // 21
  "KFMH"  // 22
});

#endif
