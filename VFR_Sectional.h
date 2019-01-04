#ifndef VFR_SECTIONAL_H
#define VFR_SECTIONAL_H

#define NUM_AIRPORTS 16 // Number of airports
#define WIND_THRESHOLD 25 // Maximum windspeed for green
#define LIGHTNING_INTERVAL 5000 // ms - how often should lightning strike; not precise because we sleep in-between
#define DO_LIGHTNING true // Lightning uses more power, but is cool.
#define DO_WINDS true // color LEDs for high winds

const char ssid[] = "CHANGE-ME";        // your network SSID (name)
const char pass[] = "CHANGE-ME";    // your network password (use for WPA, or use as key for WEP)
boolean ledStatus = true; // used so leds only indicate connection status on first boot, or after failure

#define DATA_PIN    4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 175

std::vector<unsigned short int> lightningLeds;
std::vector<String> airports({
  "KBIE", // 1
  "KAFK", // 2
  "KPMV", // 3
  "KOFF", // 4
  "KCBF", // 5
  "KOMA", // 6
  "KMLE", // 7
  "KAHQ", // 8
  "KFET", // 9
  "KBTA", // 10
  "KTQE", // 11
  "KLCG", // 12
  "KOFK", // 13
  "KOLU", // 14
  "KJYR", // 15
  "KLNK" // 16 last airport does NOT have a comma after
});

#endif
