#ifndef VFR_SECTIONAL_H
#define VFR_SECTIONAL_H

#define NUM_AIRPORTS          7         // Number of airports

#define DO_LIGHTNING                  // If defined, flash the airport LED when lightning is in the METAR
#ifdef DO_LIGHTNING
  #define LIGHTNING_INTERVAL  5       // How often the lightning animation will run, in seconds
#endif

#define WIND_THRESHOLD        25      // Wind/gust speed, above which, when VFR will change from green to yellow.  Set high to disable.

#define DO_SLEEP                      // If defined, automatically stop polling and turn off LEDs after a certain time
#ifdef DO_SLEEP
  #define SLEEP_START_ZULU    3       // The hour (UTC+0) at which the sectional will sleep
  #define SLEEP_END_ZULU      11      // The hour (UTC+0) at which the sectional will wake up
#endif

#define DO_TSL2561                    // If defined, use the TSL2561 illuminance sensor to dynamically set the LED brightness
#ifdef DO_TSL2561
  #define LUX_THRESHOLD       50      // Threshold in lux for switching between high and low brightness
  #define BRIGHTNESS_LOW      32      // Brightness when below the low level
  #define BRIGHTNESS_HIGH     128     // Brightness when below the high level
#endif

#define LED_DATA_PIN          4
#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB
#define BRIGHTNESS_DEFAULT    64      // Default brightness which optionally gets changed if DO_TSL2561 is enabled

const char ssid[] = "CHANGE-ME";      // your network SSID (name)
const char pass[] = "CHANGE-ME";      // your network password (use for WPA, or use as key for WEP)

std::vector<String> airports({
  "KBOS", // 1
  "KCQX", // 2
  "KPVC", // 3
  "KPVD", // 4
  "KFMH", // 5
  "KORH", // 6
  "KPSF"  // 7 last airport does NOT have a comma after
});

#endif
