/**
 *  ESP8266 (NodeMCU) LED sectional based on the Arduino framework.
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */
 
#include "VFR_Sectional.h"

#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <vector>

#ifdef DO_SLEEP
#include "WorldTimeAPI.h"
#endif

#ifdef DO_TSL2561
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#endif

#ifndef AW_SERVER
  #define AW_SERVER "www.aviationweather.gov"
#endif

#ifndef BASE_URI
  #define BASE_URI "/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=3&mostRecentForEachStation=true&stationString="
#endif

#define READ_TIMEOUT_S              15    // Cancel query if no data received (seconds)

#define METAR_RETRY_INTERVAL_S      15    // If fetching a METAR failed, retry again in X seconds

#ifndef METAR_REQUEST_INTERVAL_S
  #define METAR_REQUEST_INTERVAL_S  900   // in seconds (15 min is 900000)
#endif

// Array to track the current color assignments to the LED strip
CRGB leds[NUM_AIRPORTS];

uint8_t brightnessCurrent = BRIGHTNESS_DEFAULT;

#ifdef DO_LIGHTNING
std::vector<unsigned short int> lightningLeds;
#endif

#ifdef DO_SLEEP
#ifdef TIMEZONE
WorldTimeAPI wtAPI = WorldTimeAPI( WorldTimeAPI::TIME_USING_TIMEZONE, TIMEZONE );
#else
WorldTimeAPI wtAPI = WorldTimeAPI();
#endif
#endif

#ifdef DO_TSL2561
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified( TSL2561_ADDR_FLOAT, 1 );
bool tslPresent;
uint8_t brightnessTarget = BRIGHTNESS_DEFAULT;
#endif

void setup()
{
  Serial.begin( 115200 );

  // Init onboard LED to off
  pinMode( LED_BUILTIN, OUTPUT );
  digitalWrite( LED_BUILTIN, HIGH );

  // Fresh line
  Serial.println();

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
  fill_solid( leds, NUM_AIRPORTS, CRGB::Black );
  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>( leds, NUM_AIRPORTS ).setCorrection( TypicalLEDStrip );
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

#ifdef DO_SLEEP
  static bool sleeping = false;
#endif

#ifdef DO_LIGHTNING
  static unsigned long lightningLast = 0;
#endif

#ifdef DO_TSL2561
  static unsigned long tslLast = 0;
#endif

#ifdef SECTIONAL_DEBUG
  // Turn on the onboard LED
  digitalWrite( LED_BUILTIN, LOW );
#endif
  
  // Wi-Fi routine
  if( WiFi.status() != WL_CONNECTED )
  {
    String mac = WiFi.macAddress();
    int pos;
    // Strip MAC address of colons
    while( (pos = mac.indexOf(':')) >= 0 ) mac.remove( pos, 1 );

    String myHostname = "LED-Sectional-" + mac;
    
    Serial.println( "I am \"" + myHostname + "\"" );
    Serial.print( "Connecting to SSID \"" );
    Serial.print( ssid );
    Serial.print( "\"..." );

    if( metarLast == 0 )
    {
      // Show Wi-Fi is not connected with Orange across the board if a METAR report is pending so the sectional isn't left in the dark
      fill_solid( leds, NUM_AIRPORTS, CRGB::Orange );
      FastLED.show();
    }
    
    WiFi.mode( WIFI_STA );
    WiFi.hostname( myHostname );
    WiFi.begin( ssid, pass );
    
    // Wait up to 1 minute for connection...
    for( unsigned c = 0; (c < 60) && (WiFi.status() != WL_CONNECTED); c++ )
    {
      Serial.write( '.' );
      delay( 1000 );
    }
    
    if( WiFi.status() != WL_CONNECTED )
    {
      Serial.println( "Failed. Will retry..." );
      return;
    }
    
    Serial.println( "OK!" );

    if( metarLast == 0 )
    {
      // Show success with Purple across the board if a METAR report is pending so the sectional isn't left in the dark
      fill_solid( leds, NUM_AIRPORTS, CRGB::Purple );
      FastLED.show();
    }
  }

#ifdef DO_SLEEP
  // Sleep routine
  {
    if( wtAPI.update() )
    {
      Serial.print( "Time is now " );
      Serial.println( wtAPI.getFormattedTime() );
    }

    // What if the update hasn't worked?  Fail hard or gracefully?

    int hoursNow = wtAPI.getHour();
    int dayNow = wtAPI.getDay();

    bool shouldBeAsleep = false;
    
    if( dayIsWeekend[dayNow] )
    {
      shouldBeAsleep = (SLEEP_WE_START <= hoursNow) || (hoursNow < SLEEP_WE_END);
    }
    else
    {
      shouldBeAsleep = (SLEEP_WD_START <= hoursNow) || (hoursNow < SLEEP_WD_END);
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
      fill_solid( leds, NUM_AIRPORTS, CRGB::Black );
      FastLED.show();
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
    }
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

#ifdef DO_TSL2561
  // TSL2561 sensor routine
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
    fill_gradient_RGB( leds, NUM_AIRPORTS, CRGB::Red, CRGB::Blue ); // Just let us know we're running
    FastLED.show();
#endif

    Serial.println( "Getting METARs" );

    if( getMetars() )
    {
#ifdef DO_LIGHTNING
      if( lightningLeds.size() > 0 )
      {
        lightningLast = 0;
      }
#endif
      
      Serial.print( "METAR request again in " );
      Serial.print( METAR_REQUEST_INTERVAL_S );
      Serial.println( " seconds." );
      
      metarInterval = METAR_REQUEST_INTERVAL_S * 1000;
    }
    else
    {
      // Indicate error with Cyan
      fill_solid( leds, NUM_AIRPORTS, CRGB::Cyan );

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

bool getMetars()
{
#ifdef DO_LIGHTNING
  lightningLeds.clear(); // clear out existing lightning LEDs since they're global
#endif

  // Set everything to black just in case there is no report for a given airport
  fill_solid( leds, NUM_AIRPORTS, CRGB::Black );
  
  unsigned long t;
  char c;
  bool readingAirport = false;
  bool readingCondition = false;
  bool readingWind = false;
  bool readingGusts = false;
  bool readingWxstring = false;

  unsigned led = 99;
  String currentAirport = "";
  String currentCondition = "";
  String currentLine = "";
  String currentWind = "";
  String currentGusts = "";
  String currentWxstring = "";
  String airportString = "";

  for( unsigned i = 0; i < (NUM_AIRPORTS); i++ )
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

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  Serial.println( "\nStarting connection to server..." );
  // if you get a connection, report back via serial:
  if( !client.connect(AW_SERVER, 443) )
  {
    Serial.println( "Connection failed!" );
    client.stop();
    return false;
  }
  else
  {
    Serial.print( "Connected.  Requesting data..." );
#ifdef SECTIONAL_DEBUG
    Serial.print( "GET " );
    Serial.print( BASE_URI );
    Serial.print( airportString );
    Serial.println( " HTTP/1.1" );
    Serial.print( "Host: " );
    Serial.println( AW_SERVER );
    Serial.println( "Connection: close" );
    Serial.println();
#endif
    // Make a HTTP request, and print it to console:
    client.print( "GET " );
    client.print( BASE_URI );
    client.print( airportString );
    client.println( " HTTP/1.1" );
    client.print( "Host: " );
    client.println( AW_SERVER );
    client.println( "Connection: close" );
    client.println();
    client.flush();
    
    t = millis(); // start time
    FastLED.clear();

    while( !client.connected() )
    {
      if( (millis() - t) >= (READ_TIMEOUT_S * 1000) )
      {
        Serial.println( "---Timeout---" );
        client.stop();
        return false;
      }
      Serial.print( "." );
      delay( 1000 );
    }

    Serial.println();

    Serial.println( "Receiving data..." );

    while( client.connected() )
    {
      if( (c = client.read()) >= 0 )
      {
        yield(); // Otherwise the WiFi stack can crash
        currentLine += c;
        if( c == '\n' ) currentLine = "";
        if( currentLine.endsWith("<station_id>") )
        { 
          // start paying attention
          // we assume we are recording results at each change in airport; 99 means no airport
          if( led != 99 )
          { 
            doColor( currentAirport, led, currentWind.toInt(), currentGusts.toInt(), currentCondition, currentWxstring );
          }
          currentAirport = ""; // Reset everything when the airport changes
          readingAirport = true;
          currentCondition = "";
          currentWind = "";
          currentGusts = "";
          currentWxstring = "";
        }
        else if( readingAirport )
        {
          if( !currentLine.endsWith("<") )
          {
            currentAirport += c;
          } 
          else
          {
            readingAirport = false;
            led = 99;
            for( unsigned i = 0; i < NUM_AIRPORTS; i++ )
            {
              if( airports[i] == currentAirport ) led = i;
            }
          }
        }
        else if( currentLine.endsWith("<wind_speed_kt>") )
        {
          readingWind = true;
        } 
        else if( readingWind )
        {
          if( !currentLine.endsWith("<") )
          {
            currentWind += c;
          }
          else
          {
            readingWind = false;
          }
        }
        else if( currentLine.endsWith("<wind_gust_kt>") )
        {
          readingGusts = true;
        }
        else if( readingGusts )
        {
          if( !currentLine.endsWith("<") )
          {
            currentGusts += c;
          }
          else
          {
            readingGusts = false;
          }
        }
        else if( currentLine.endsWith("<flight_category>") )
        {
          readingCondition = true;
        }
        else if( readingCondition )
        {
          if( !currentLine.endsWith("<") )
          {
            currentCondition += c;
          }
          else
          {
            readingCondition = false;
          }
        }
        else if( currentLine.endsWith("<wx_string>") )
        {
          readingWxstring = true;
        }
        else if( readingWxstring )
        {
          if( !currentLine.endsWith("<") )
          {
            currentWxstring += c;
          }
          else
          {
            readingWxstring = false;
          }
        }
        t = millis(); // Reset timeout clock
      }
      else if( (millis() - t) >= (READ_TIMEOUT_S * 1000) )
      {
        Serial.println( "---Timeout---" );
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
  doColor( currentAirport, led, currentWind.toInt(), currentGusts.toInt(), currentCondition, currentWxstring );
  
  // Do the key LEDs now if they exist
  for( unsigned i = 0; i < (NUM_AIRPORTS); i++ )
  {
    // Use this opportunity to set colors for LEDs in our key then build the request string
    if( airports[i] == "VFR" ) leds[i] = CRGB::Green;
    else if( airports[i] == "WVFR" ) leds[i] = CRGB::Yellow;
    else if( airports[i] == "MVFR" ) leds[i] = CRGB::Blue;
    else if( airports[i] == "IFR" ) leds[i] = CRGB::Red;
    else if( airports[i] == "LIFR" ) leds[i] = CRGB::Magenta;
  }

  client.stop();
  return true;
}

void doColor( String identifier, unsigned short int led, int wind, int gusts, String condition, String wxstring )
{
  CRGB color;
  Serial.print( identifier );
  Serial.print( ": " );
  Serial.print( condition );
  Serial.print( " " );
  Serial.print( wind );
  Serial.print( "G" );
  Serial.print( gusts );
  Serial.print( "kts LED " );
  Serial.print( led );
  Serial.print( " WX: " );
  Serial.println( wxstring );

#ifdef DO_LIGHTNING
  if( wxstring.indexOf("TS") != -1 )
  {
    Serial.println( "... found lightning!" );
    lightningLeds.push_back( led );
  }
#endif

  if( condition == "LIFR" )
  {
    color = CRGB::Magenta;
  }
  else if( condition == "IFR" )
  {
    color = CRGB::Red;
  }
  else if( condition == "MVFR" )
  {
    color = CRGB::Blue;
  }
  else if( condition == "VFR" )
  {
    if( (wind > WIND_THRESHOLD) || (gusts > WIND_THRESHOLD) )
    {
      color = CRGB::Yellow;
    }
    else
    {
      color = CRGB::Green;
    }
  }
  else if( condition.length() == 0 )
  {
    // We should indicate that an airport was in the result without a flight category
    color = CRGB::White;
  }
  else
  {
    color = CRGB::Black;
  }

  leds[led] = color;
}
