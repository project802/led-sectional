/* If using the NODEMCU and WS2812B, plug the WS2812B LEDs into the Vin and Ground directly, and the data pin */



#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <vector>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "VFR_Sectional.h"

using namespace std;

#define FASTLED_ESP8266_RAW_PIN_ORDER

#ifndef AW_SERVER
  #define AW_SERVER "www.aviationweather.gov"
#endif

#ifndef BASE_URI
  #define BASE_URI "/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=3&mostRecentForEachStation=true&stationString="
#endif

#define READ_TIMEOUT      15      // Cancel query if no data received (seconds)
#define WIFI_TIMEOUT      60      // in seconds

#define METAR_RETRY_INTERVAL    15000   // in ms
#ifndef METAR_REQUEST_INTERVAL
  #define METAR_REQUEST_INTERVAL  900000  // in ms (15 min is 900000)
#endif

std::vector<unsigned short int> lightningLeds;

// Define the array of leds
CRGB leds[NUM_AIRPORTS];

WiFiUDP ntpUDP;
NTPClient timeClient( ntpUDP );

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  //pinMode(D1, OUTPUT); //Declare Pin mode
  //while (!Serial) {
  //    ; // wait for serial port to connect. Needed for native USB
  //}

  pinMode(LED_BUILTIN, OUTPUT); // give us control of the onboard LED
  digitalWrite(LED_BUILTIN, LOW);

  // Initialize LEDs
  fill_solid(leds, NUM_AIRPORTS, CRGB::Black);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_AIRPORTS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  // Do a double 'show' to get the LEDs into a known good state.  Depending on the behavior of the
  // data pin during boot and configuration, it can cause the first LED to be "skipped" and left in
  // the default configuration.
  FastLED.show();
  FastLED.show();

  // Set the NTP client to update with the server only every 24 hours, not the default every hour
  timeClient.setUpdateInterval( 24 * 60 * 60 * 1000 );
  timeClient.begin();
}

void loop() {
  static bool sleeping = false;
  static unsigned long metarLast = 0;
  static unsigned long metarInterval = METAR_REQUEST_INTERVAL;
  static unsigned long lightningLast = 0;
  
  digitalWrite( LED_BUILTIN, LOW ); // on if we're awake
  
  // Connect to WiFi. We always want a wifi connection for the ESP8266
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    fill_solid(leds, NUM_AIRPORTS, CRGB::Orange); // indicate status with LEDs, but only on first run or error
    FastLED.show();
    WiFi.mode(WIFI_STA);
    WiFi.hostname("LED Sectional " + WiFi.macAddress());
    //wifi_set_sleep_type(LIGHT_SLEEP_T); // use light sleep mode for all delays
    Serial.print( "I am " );
    Serial.println( WiFi.macAddress() );
    Serial.print("WiFi connecting to SSID ");
    Serial.print( ssid );
    Serial.print( "..." );
    WiFi.begin(ssid, pass);
    // Wait up to 1 minute for connection...
    for (unsigned int c = 0; (c < WIFI_TIMEOUT) && (WiFi.status() != WL_CONNECTED); c++) {
      Serial.write('.');
      delay(1000);
    }
    if (WiFi.status() != WL_CONNECTED) { // If it didn't connect within WIFI_TIMEOUT
      Serial.println("Failed. Will retry...");
      fill_solid(leds, NUM_AIRPORTS, CRGB::Orange);
      FastLED.show();
      return;
    }
    Serial.println("OK!");
    fill_solid(leds, NUM_AIRPORTS, CRGB::Purple); // indicate status with LEDs
    FastLED.show();
  }

  timeClient.update();
  
  if( DO_SLEEP )
  {
    int hoursNow = timeClient.getHours();

    bool shouldBeAsleep = (SLEEP_START_ZULU <= hoursNow) && (hoursNow < SLEEP_END_ZULU);

    if( !sleeping && shouldBeAsleep )
    {
      Serial.println( "Time for bed!" );
      sleeping = true;

      // Turn off METAR LEDs
      fill_solid(leds, NUM_AIRPORTS, CRGB::Black);
      FastLED.show();
    }
    else if( sleeping && !shouldBeAsleep )
    {
      Serial.println( "Time to wake up!" );
      sleeping = false;

      // Reset METAR timer
      metarLast = 0;
    }
  }

  if( sleeping )
  {
    digitalWrite( LED_BUILTIN, HIGH );
    delay( 60 * 1000 );
    return;
  }

  if( (metarLast == 0) || (millis() - metarLast > metarInterval ) )
  {
    if( SECTIONAL_DEBUG )
    {
      fill_gradient_RGB(leds, NUM_AIRPORTS, CRGB::Red, CRGB::Blue); // Just let us know we're running
      FastLED.show();
    }

    Serial.print("Getting METARs at ");
    Serial.println( timeClient.getFormattedTime() );
    
    metarLast = millis();
    
    if( getMetars() )
    {
      Serial.println("Refreshing LEDs.");
      FastLED.show();
      
      if( DO_LIGHTNING && lightningLeds.size() > 0 )
      {
        lightningLast = 0;
      }
      
      Serial.print( "METAR request again in " );
      Serial.println( METAR_REQUEST_INTERVAL);
      
      metarInterval = METAR_REQUEST_INTERVAL;
    }
    else
    {
      Serial.print( "METAR fetch failed.  Retry in " );
      Serial.println( METAR_RETRY_INTERVAL );
      fill_solid( leds, NUM_AIRPORTS, CRGB::Cyan ); // indicate status with LEDs
      FastLED.show();
      metarInterval = METAR_RETRY_INTERVAL;
    }
  }

  // Do some lightning
  if( DO_LIGHTNING && (lightningLeds.size() > 0) && (millis() - lightningLast > (LIGHTNING_INTERVAL*1000)) )
  {
    std::vector<CRGB> lightning(lightningLeds.size());
    
    for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
      unsigned short int currentLed = lightningLeds[i];
      lightning[i] = leds[currentLed]; // temporarily store original color
      leds[currentLed] = CRGB::White; // set to white briefly
    }
    FastLED.show();
    
    delay(25);
    
    for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
      unsigned short int currentLed = lightningLeds[i];
      leds[currentLed] = lightning[i]; // restore original color
    }
    FastLED.show();
    
    lightningLast = millis() - 10;
  }
  
  digitalWrite( LED_BUILTIN, HIGH );
  delay( 1000 );
}

bool getMetars(){
  lightningLeds.clear(); // clear out existing lightning LEDs since they're global
  fill_solid(leds, NUM_AIRPORTS, CRGB::Black); // Set everything to black just in case there is no report
  uint32_t t;
  char c;
  boolean readingAirport = false;
  boolean readingCondition = false;
  boolean readingWind = false;
  boolean readingGusts = false;
  boolean readingWxstring = false;

  unsigned short int led = 99;
  String currentAirport = "";
  String currentCondition = "";
  String currentLine = "";
  String currentWind = "";
  String currentGusts = "";
  String currentWxstring = "";
  String airportString = "";
  bool firstAirport = true;
  for (int i = 0; i < (NUM_AIRPORTS); i++) {
    if (airports[i] != "NULL" && airports[i] != "VFR" && airports[i] != "MVFR" && airports[i] != "WVFR" && airports[i] != "IFR" && airports[i] != "LIFR") {
      if (firstAirport) {
        firstAirport = false;
        airportString = airports[i];
      } else airportString = airportString + "," + airports[i];
    }
  }

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (!client.connect(AW_SERVER, 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return false;
  } else {
    Serial.println("Connected ...");
    Serial.print("GET ");
    Serial.print(BASE_URI);
    Serial.print(airportString);
    Serial.println(" HTTP/1.1");
    Serial.print("Host: ");
    Serial.println(AW_SERVER);
    Serial.println("Connection: close");
    Serial.println();
    // Make a HTTP request, and print it to console:
    client.print("GET ");
    client.print(BASE_URI);
    client.print(airportString);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(AW_SERVER);
    client.println("Connection: close");
    client.println();
    client.flush();
    t = millis(); // start time
    FastLED.clear();

    Serial.print("Getting data");

    while (!client.connected()) {
      if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        client.stop();
        return false;
      }
      Serial.print(".");
      delay(1000);
    }

    Serial.println();

    while (client.connected()) {
      if ((c = client.read()) >= 0) {
        yield(); // Otherwise the WiFi stack can crash
        currentLine += c;
        if (c == '\n') currentLine = "";
        if (currentLine.endsWith("<station_id>")) { // start paying attention
          if (led != 99) { // we assume we are recording results at each change in airport; 99 means no airport
            doColor(currentAirport, led, currentWind.toInt(), currentGusts.toInt(), currentCondition, currentWxstring);
          }
          currentAirport = ""; // Reset everything when the airport changes
          readingAirport = true;
          currentCondition = "";
          currentWind = "";
          currentGusts = "";
          currentWxstring = "";
        } else if (readingAirport) {
          if (!currentLine.endsWith("<")) {
            currentAirport += c;
          } else {
            readingAirport = false;
            for (unsigned short int i = 0; i < NUM_AIRPORTS; i++) {
              if (airports[i] == currentAirport) {
                led = i;
              }
            }
          }
        } else if (currentLine.endsWith("<wind_speed_kt>")) {
          readingWind = true;
        } else if (readingWind) {
          if (!currentLine.endsWith("<")) {
            currentWind += c;
          } else {
            readingWind = false;
          }
        } else if (currentLine.endsWith("<wind_gust_kt>")) {
          readingGusts = true;
        } else if (readingGusts) {
          if (!currentLine.endsWith("<")) {
            currentGusts += c;
          } else {
            readingGusts = false;
          }
        } else if (currentLine.endsWith("<flight_category>")) {
          readingCondition = true;
        } else if (readingCondition) {
          if (!currentLine.endsWith("<")) {
            currentCondition += c;
          } else {
            readingCondition = false;
          }
        } else if (currentLine.endsWith("<wx_string>")) {
          readingWxstring = true;
        } else if (readingWxstring) {
          if (!currentLine.endsWith("<")) {
            currentWxstring += c;
          } else {
            readingWxstring = false;
          }
        }
        t = millis(); // Reset timeout clock
      } else if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        client.stop();
        return false;
      }
    }
  }

  if( led == 99 )
  {
    Serial.println( "Error! No airports found!" );
    // [todo] indicate an error with the LEDs?
    client.stop();
    return false;
  }
  
  // need to doColor this for the last airport
  doColor(currentAirport, led, currentWind.toInt(), currentGusts.toInt(), currentCondition, currentWxstring);
  
  // Do the key LEDs now if they exist
  for (int i = 0; i < (NUM_AIRPORTS); i++) {
    // Use this opportunity to set colors for LEDs in our key then build the request string
    if (airports[i] == "VFR") leds[i] = CRGB::Green;
    else if (airports[i] == "WVFR") leds[i] = CRGB::Yellow;
    else if (airports[i] == "MVFR") leds[i] = CRGB::Blue;
    else if (airports[i] == "IFR") leds[i] = CRGB::Red;
    else if (airports[i] == "LIFR") leds[i] = CRGB::Magenta;
  }

  client.stop();
  return true;
}

void doColor(String identifier, unsigned short int led, int wind, int gusts, String condition, String wxstring) {
  CRGB color;
  Serial.print(identifier);
  Serial.print(": ");
  Serial.print(condition);
  Serial.print(" ");
  Serial.print(wind);
  Serial.print("G");
  Serial.print(gusts);
  Serial.print("kts LED ");
  Serial.print(led);
  Serial.print(" WX: ");
  Serial.println(wxstring);
  if (wxstring.indexOf("TS") != -1) {
    Serial.println("... found lightning!");
    lightningLeds.push_back(led);
  }
  if (condition == "LIFR" || identifier == "LIFR") color = CRGB::Magenta;
  else if (condition == "IFR") color = CRGB::Red;
  else if (condition == "MVFR") color = CRGB::Blue;
  else if (condition == "VFR") {
    if ((wind > WIND_THRESHOLD || gusts > WIND_THRESHOLD) && DO_WINDS) {
      color = CRGB::Yellow;
    } else {
      color = CRGB::Green;
    }
  } else color = CRGB::Black;

  leds[led] = color;
}
