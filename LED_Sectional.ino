/**
 *  ESP8266 (NodeMCU) LED sectional based on the Arduino framework.
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */

#include "LED_Sectional.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FastLED.h>
#include <vector>
#include "tinyxml2.h"

#ifdef DO_SLEEP
#include "WorldTimeAPI.h"
#endif

#ifdef DO_TSL2561
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#endif

using namespace tinyxml2;

#ifndef AW_SERVER
  #define AW_SERVER                 "www.aviationweather.gov"
#endif

#ifndef BASE_URI
  #define BASE_URI                  "/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=3&mostRecentForEachStation=true&stationString="
#endif

#define READ_TIMEOUT_S              15      // Cancel query if no data received (seconds)

#define METAR_RETRY_INTERVAL_S      15      // If fetching a METAR failed, retry again in X seconds

#ifndef METAR_REQUEST_INTERVAL_S
  #define METAR_REQUEST_INTERVAL_S  (15*60) // Time between METAR fetches in seconds
#endif

// Array to track the current color assignments to the LED strip
CRGB *leds;

uint8_t brightnessCurrent =         BRIGHTNESS_DEFAULT;

#ifdef DO_LIGHTNING
std::vector<unsigned short int> lightningLeds;
#endif

#ifdef DO_TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified( TSL2561_ADDR_FLOAT, 1 );
bool tslPresent;
uint8_t brightnessTarget = BRIGHTNESS_DEFAULT;
#endif

void setup()
{
  String mac = WiFi.macAddress();
  int pos;
  // Strip MAC address of colons
  while( (pos = mac.indexOf(':')) >= 0 ) mac.remove( pos, 1 );

  String myHostname = "LED-Sectional-" + mac;
  
  WiFi.hostname( myHostname );

  Serial.begin( 115200 );
  
   // Fresh line
  Serial.println();
  Serial.println( "I am \"" + myHostname + "\"" );
  
  leds = (CRGB *) malloc( sizeof(CRGB) * airports.size() );
  if( leds == NULL )
  {
    Serial.println( "Unable to allocate memory for leds!" );
    while(1) delay(1);
  }
  
  // Init onboard LED to off
  pinMode( LED_BUILTIN, OUTPUT );
  digitalWrite( LED_BUILTIN, HIGH );

#ifdef DO_TSL2561
  if( !tsl.begin() )
  {
    Serial.println( "Unable to find TSL2561 lux sensor.  Check sensor address." ); 
    tslPresent = false;
  }
  else
  {
    Serial.println( "TSL2561 found." );
    tslPresent = true;

    tsl.enableAutoRange( true );
    tsl.setIntegrationTime( TSL2561_INTEGRATIONTIME_402MS );
  }
#endif

  // Initialize METAR LEDs
  fill_solid( leds, airports.size(), CRGB::Black );
  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>( leds, airports.size() ).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( brightnessCurrent );

  // Do a double 'show' to get the LEDs into a known good state.  Depending on the behavior of the
  // data pin during boot and configuration, it can cause the first LED to be "skipped" and left in
  // the default configuration.
  FastLED.show();
  FastLED.show();
}

void loop()
{
  static unsigned long metarLast = 0;
  static unsigned long metarInterval = METAR_REQUEST_INTERVAL_S * 1000;

#ifdef DO_LIGHTNING
  static unsigned long lightningLast = 0;
#endif

#ifdef SECTIONAL_DEBUG
  // Turn on the onboard LED
  digitalWrite( LED_BUILTIN, LOW );
#endif

#ifdef DO_SLEEP
  // Sleep routine
  do
  {
#ifdef TIMEZONE
    static WorldTimeAPI wtAPI = WorldTimeAPI( WorldTimeAPI::TIME_USING_TIMEZONE, TIMEZONE );
#else
    static WorldTimeAPI wtAPI = WorldTimeAPI();
#endif
    static bool sleeping = false;
    
    // Only update the time if we aren't asleep and on Wi-Fi.
    // Sleeping disconnects Wi-Fi so we don't want the corner case where we are off Wi-Fi
    // and a time update fails so timeReceived returns false and we never wake up due to 
    // a lack of time!  Since sleeping isn't true until we have time, this is guaranteed
    // to not have a startup conflict.  If the update period expires while we are asleep
    // thats ok we will wake up according to the last time update then sync again first thing.
    if( !sleeping && (WiFi.status() == WL_CONNECTED) )
    {
      if( wtAPI.update() )
      {
        Serial.print( "Time is now " );
        Serial.print( wtAPI.getFormattedTime() );
        Serial.print( ", day " );
        Serial.println( wtAPI.getDayOfYear() );
      }
    }

    // Without any valid time, we can't appropriately know when to sleep and wake up so abort.
    if( !wtAPI.timeReceived() )
    {
      break;
    }
    
    int hourNow = wtAPI.getHour();
    int dayNow = wtAPI.getDayOfWeek();

    bool dayIsHoliday = false;
    for( unsigned i = 0; i < holidays.size(); i++ )
    {
      if( holidays[i] == wtAPI.getDayOfYear() )
        dayIsHoliday = true;
    }
    
    bool shouldBeAsleep = false;
    
    if( dayIsWeekend[dayNow] || dayIsHoliday )
    {
      shouldBeAsleep = (SLEEP_WE_START <= hourNow) || (hourNow < SLEEP_WE_END);
    }
    else
    {
      shouldBeAsleep = (SLEEP_WD_START <= hourNow) || (hourNow < SLEEP_WD_END);
    }
    
    if( !sleeping && shouldBeAsleep )
    {
#ifdef SECTIONAL_DEBUG
      Serial.print( wtAPI.getFormattedTime() );
      Serial.print( " time for bed! dayNow:" );
      Serial.print( dayNow );
      Serial.print( " dayIsWeekend:" );
      Serial.println( dayIsWeekend[dayNow] );
#endif
      sleeping = true;

      // Turn off METAR LEDs
      fill_solid( leds, airports.size(), CRGB::Black );
      FastLED.show();

      // Don't use the disconnect() function which changes the config, clearing the SSID & password
      WiFi.mode( WIFI_OFF );
      while( WiFi.status() == WL_CONNECTED ) delay(0);
      WiFi.enableSTA( false );
      WiFi.forceSleepBegin();
    }
    else if( sleeping && !shouldBeAsleep )
    {
#ifdef SECTIONAL_DEBUG
      Serial.print( wtAPI.getFormattedTime() );
      Serial.println( " time to wake up!" );
#endif
      sleeping = false;

      // Reset METAR timer
      metarLast = 0;

      WiFi.forceSleepWake();
      WiFi.enableSTA( true );
      WiFi.mode( WIFI_STA );
      WiFi.begin();
    }

    if( sleeping )
    {
#ifdef SECTIONAL_DEBUG
      digitalWrite( LED_BUILTIN, HIGH );
#endif
      delay( 60 * 1000 );
      return;
    }
#endif
  }
  while( 0 );

  // Wi-Fi routine
  if( WiFi.status() != WL_CONNECTED )
  {
    // By default, the ESP8266 will use the stored wifi credentials to reconnect to wifi.
    // Only use the ones programmatically assigned here if there is a failure to connect or none set

    if( WiFi.SSID().length() == 0 )
    {
      Serial.println( "No WiFi config found.  Starting." );
      WiFi.mode( WIFI_STA );
      WiFi.begin( ssid, pass );
    }
    
    Serial.print( "Connecting to SSID \"" );
    Serial.print( WiFi.SSID() );
    Serial.print( "\"..." );

    if( metarLast == 0 )
    {
      // Show Wi-Fi is not connected with Orange across the board if a METAR report is pending so the sectional isn't left in the dark
      fill_solid( leds, airports.size(), CRGB::Orange );
      FastLED.show();
    }
    
    // Wait up to 1 minute for connection...
    for( unsigned c = 0; (c < 60) && (WiFi.status() != WL_CONNECTED); c++ )
    {
      Serial.write( '.' );
      delay( 1000 );
    }
    
    if( WiFi.status() != WL_CONNECTED )
    {
      Serial.println( "Failed. Will retry..." );
      WiFi.mode( WIFI_STA );
      WiFi.begin( ssid, pass );
      return;
    }
    
    Serial.println( "OK!" );

    if( metarLast == 0 )
    {
      // Show success with Purple across the board if a METAR report is pending so the sectional isn't left in the dark
      fill_solid( leds, airports.size(), CRGB::Purple );
      FastLED.show();
    }
  }

#ifdef DO_TSL2561
  // TSL2561 sensor routine
  static unsigned long tslLast = 0;
  if( tslPresent && (millis() - tslLast > 5000) )
  {
    sensors_event_t event;

    tsl.getEvent( &event );
  
    for( unsigned i = 0; luxMap[i] != NULL; i++ )
    {
      if( event.light < luxMap[i][0] )
      {
        float slope = (float)(luxMap[i][1] - luxMap[i-1][1]) / (float)(luxMap[i][0] - luxMap[i-1][0]);

        float result = luxMap[i-1][1] + ((event.light - luxMap[i-1][0]) * slope);

        if( result <= (uint8_t)(~0) )
        {
          brightnessTarget = (uint8_t) result;
        }
        else
        {
          brightnessTarget = (uint8_t)(~0);
        }

        // Even numbers only, please.  Makes ramping by 2 easy with less conditional math.
        brightnessTarget = brightnessTarget & ~((uint8_t)1);

        // Shortcut values less than 3 to 0.  It doesn't have even color representation any longer.
        if( brightnessTarget < 3 )
        {
          brightnessTarget = 0;
        }
        
#ifdef SECTIONAL_DEBUG
        Serial.print( "TSL2561: " );
        Serial.print( event.light );
        Serial.print( " between " );
        Serial.print( luxMap[i-1][0] );
        Serial.print( " and " );
        Serial.print( luxMap[i][0] );
        Serial.print( ", slope " );
        Serial.print( slope );
        Serial.print( ", reuslt " );
        Serial.println( result );
#endif  
        break;
      }
    }
    
    tslLast = millis();
  }

  if( brightnessCurrent != brightnessTarget )
  {
#ifdef SECTIONAL_DEBUG
    Serial.print( "TSL2561: current " );
    Serial.print( brightnessCurrent );
    Serial.print( " target " );
    Serial.println( brightnessTarget );
#endif

    brightnessCurrent += brightnessCurrent < brightnessTarget ? 2 : -2;
    
    FastLED.setBrightness( brightnessCurrent );
    FastLED.show();
  }
#endif

  // Metar routine
  if( (metarLast == 0) || (millis() - metarLast > metarInterval ) )
  {
#ifdef SECTIONAL_DEBUG
    fill_gradient_RGB( leds, airports.size(), CRGB::Red, CRGB::Blue ); // Just let us know we're running
    FastLED.show();
#endif

    Serial.println( "Getting METARs" );

    if( getMetars() )
    {
#ifdef DO_LIGHTNING
      lightningLast = 0;
#endif
      
      Serial.print( "METAR request again in " );
      Serial.print( METAR_REQUEST_INTERVAL_S );
      Serial.println( " seconds." );
      
      metarInterval = METAR_REQUEST_INTERVAL_S * 1000;
    }
    else
    {
      // Indicate error with Cyan
      fill_solid( leds, airports.size(), CRGB::Cyan );

      Serial.print( "METAR fetch failed.  Retry in " );
      Serial.print( METAR_RETRY_INTERVAL_S );
      Serial.println( " seconds." );
      
      metarInterval = METAR_RETRY_INTERVAL_S * 1000;
    }

    FastLED.show();

    metarLast = millis();
  }

#ifdef DO_LIGHTNING
  // Lightning routine
  if( (lightningLeds.size() > 0) && (millis() - lightningLast > (LIGHTNING_INTERVAL*1000)) )
  {
    std::vector<CRGB> lightning( lightningLeds.size() );
    
    for( unsigned i = 0; i < lightningLeds.size(); ++i )
    {
      unsigned currentLed = lightningLeds[i];
      lightning[i] = leds[currentLed]; // temporarily store original color
      leds[currentLed] = CRGB::White; // set to white briefly
    }
    FastLED.show();
    
    delay( 25 );
    
    for( unsigned i = 0; i < lightningLeds.size(); ++i )
    {
      unsigned currentLed = lightningLeds[i];
      leds[currentLed] = lightning[i]; // restore original color
    }
    FastLED.show();
    
    lightningLast = millis() - 10;
  }
#endif

#ifdef SECTIONAL_DEBUG
  digitalWrite( LED_BUILTIN, HIGH );
#endif

  // All done.  Yeild to other processes.
  delay( 1000 );
}

bool parseMetarAndAssignLed( const char *xml )
{
  XMLDocument doc;
  
  XMLError result = doc.Parse( xml );

  if( result != XML_SUCCESS )
  {
    return false;
  }

  XMLNode* metar = doc.FirstChildElement( "METAR" );
  if( metar == NULL )
  {
    return false;
  }

  XMLElement* stationIdElem = metar->FirstChildElement( "station_id" );
  XMLElement* flightCategoryElem = metar->FirstChildElement( "flight_category" );
  XMLElement* windSpeedKtElem = metar->FirstChildElement( "wind_speed_kt" );
  XMLElement* windGustKtElem = metar->FirstChildElement( "wind_gust_kt" );
  XMLElement* wxStringElem = metar->FirstChildElement( "wx_string" );
  
  if( stationIdElem == NULL )
  {
    return false;
  }

  String stationId = String( stationIdElem->GetText() );
  String flightCategory = String( flightCategoryElem != NULL ? flightCategoryElem->GetText() : "" );
  String wxString = String( wxStringElem != NULL ? wxStringElem->GetText() : "" );

  int windSpeedKt = 0;
  if( windSpeedKtElem != NULL )
  {
    windSpeedKtElem->QueryIntText( &windSpeedKt );
  }

  int windGustKt = 0;
  if( windGustKtElem != NULL )
  {
    windGustKtElem->QueryIntText( &windGustKt );
  }

  struct FlightCatColor
  {
    String category;
    CRGB color;
  };

  static FlightCatColor flightCatColors[] = 
  {
    { "LIFR", CRGB::Magenta },
    { "IFR",  CRGB::Red },
    { "MVFR", CRGB::Blue },
    { "VFR",  CRGB::Green },
    { "",     CRGB::White }
  };

  CRGB color = CRGB::Black;

  for( unsigned i = 0; i < (sizeof(flightCatColors) / sizeof(struct FlightCatColor)); i++ )
  {
    if( flightCatColors[i].category == flightCategory )
    {
      color = flightCatColors[i].color;
    }
  }
  
  if( flightCategory == "VFR" && ((windSpeedKt > WIND_THRESHOLD) || (windGustKt > WIND_THRESHOLD)) )
  {
    color = CRGB::Yellow;
  }
  
  for( unsigned i = 0; i < airports.size(); i++ )
  {
    if( airports[i] == stationId )
    {
      leds[i] = color;
      
#ifdef DO_LIGHTNING
      if( wxString.indexOf("TS") != -1 )
      {
        lightningLeds.push_back( i );
      }
#endif

#ifdef SECTIONAL_DEBUG
      Serial.print( stationId + ": " + flightCategory + " " );
      Serial.print( windSpeedKt );
      Serial.print( "G" );
      Serial.print( windGustKt );
      Serial.print( " LED " );
      Serial.print( i );
      Serial.print( " " );
      Serial.println( wxString );
#endif
    }
  }

  return true;
}

bool getMetars()
{
  String airportString = "";
  
  for( unsigned i = 0; i < (airports.size()); i++ )
  {
    if( airports[i] != "NULL" && airports[i] != "VFR" && airports[i] != "MVFR" && airports[i] != "WVFR" && airports[i] != "IFR" && airports[i] != "LIFR" )
    {
      if( airportString.length() == 0 )
      {
        airportString = airports[i];
      }
      else
      {
        airportString = airportString + "," + airports[i];
      }
    }
  }

#ifdef DO_LIGHTNING
  lightningLeds.clear(); // clear out existing lightning LEDs since they're global
#endif

  // Set everything to black just in case there is no report for a given airport
  fill_solid( leds, airports.size(), CRGB::Black );
  
  WiFiClientSecure client;
  client.setBufferSizes( 2048, 512 );
  client.setInsecure();

  Serial.print( "Starting connection to server..." );
  if( !client.connect(AW_SERVER, 443) )
  {
    Serial.println( "Connection failed!" );
    return false;
  }
  
#ifdef SECTIONAL_DEBUG
  Serial.println( String("GET ") + BASE_URI + airportString + " HTTP/1.1\r\n" +
                "Host: " + AW_SERVER + "\r\n" +
                "Connection: close\r\n\r\n" );
#endif
  client.print( String("GET ") + BASE_URI + airportString + " HTTP/1.1\r\n" +
                "Host: " + AW_SERVER + "\r\n" +
                "Connection: close\r\n\r\n" );
  client.flush();

  // Give some time for the response to start
  unsigned long timeNow = millis();
  while( !client.available() )
  {
    if( millis() - timeNow > (READ_TIMEOUT_S  * 1000) )
    {
      // Timeout
      break;
    }
    Serial.print( "." );
    delay( 1000 );
  }
  Serial.println();

  bool fullResponse = false;

  // Manually extract the <METAR> tag one at a time
  // This allows for a large result for high LED count displays without having to 
  // statically allocate high amounts of RAM to read the entire API result at once
  while( client.connected() || client.available() )
  {
    delay( 0 );
    
    String buffer = client.readStringUntil( '>' ) + ">";

    // Timeout
    if( buffer == ">" )
    {
      break;
    }

    if( buffer.endsWith("<METAR>") )
    {
      String metar = "<METAR>";
      
      while( !metar.endsWith("</METAR>") )
      {
        delay( 0 );
        metar += client.readStringUntil( '>' ) + ">";;
      }
      
      parseMetarAndAssignLed( metar.c_str() );
    }
    else if( buffer.endsWith("</response>") )
    {
      fullResponse = true;
      break;
    }
  }

  // Do the legend LEDs now if they exist
  for( unsigned i = 0; i < (airports.size()); i++ )
  {
    if( airports[i] == "VFR" ) leds[i] = CRGB::Green;
    else if( airports[i] == "WVFR" ) leds[i] = CRGB::Yellow;
    else if( airports[i] == "MVFR" ) leds[i] = CRGB::Blue;
    else if( airports[i] == "IFR" ) leds[i] = CRGB::Red;
    else if( airports[i] == "LIFR" ) leds[i] = CRGB::Magenta;
  }
  
  return fullResponse;
}
