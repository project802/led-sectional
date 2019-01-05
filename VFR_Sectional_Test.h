#ifndef VFR_SECTIONAL_H
#define VFR_SECTIONAL_H

#define NUM_AIRPORTS        7       // Number of airports

#define DO_LIGHTNING        true    // If true, flash the airport LED when lightning is in the METAR
#define LIGHTNING_INTERVAL  2       // How often the lightning animation will run, in seconds

#define DO_WINDS            true    // Change color based on wind/gust speed?
#define WIND_THRESHOLD      25      // Wind/gust speed, above which, when VFR will change from green to yellow if DO_WINDS is true

#define DO_SLEEP            true    // Automatically stop polling and turn off LEDs after a certain time?
#define SLEEP_START_ZULU    3       // The hour (UTC+0) at which the sectional will sleep
#define SLEEP_END_ZULU      11      // The hour (UTC+0) at which the sectional will wake up

#define DATA_PIN            4
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define BRIGHTNESS          30

const char ssid[] = "CHANGE-ME";    // your network SSID (name)
const char pass[] = "CHANGE-ME";   // your network password (use for WPA, or use as key for WEP)

std::vector<String> airports({
  "KBOS", // 1
  "KCQX", // 2
  "KPVC", // 3
  "KPVD", // 4
  "KFMH", // 5
  "KORH", // 6
  "KPSF" // 7 last airport does NOT have a comma after
});

#define AW_SERVER "project802.net"
#define BASE_URI "/hosted/led_sectional_test_data?stationString="
#define METAR_REQUEST_INTERVAL  (30*1000)

#define SECTIONAL_DEBUG

#endif
