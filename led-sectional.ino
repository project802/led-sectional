/**
 *  ESP8266 (NodeMCU) LED sectional based on the Arduino framework.
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */

#include "led-sectional.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <ArduinoJson.h>
#include <NeoPixelBus.h>
#include "WorldTimeAPI.h"

#ifndef AW_SERVER
  #define AW_SERVER             "aviationweather.gov"
#endif

#ifndef BASE_URI
  #define BASE_URI              "api/data/metar?format=geojson&taf=false&ids="
#endif

const unsigned                  METAR_READ_TIMEOUT_S      = 5;
const unsigned                  METAR_RETRY_INTERVAL_S    = 5;
const unsigned                  METAR_REQUEST_INTERVAL_S  = (15*60);
const unsigned                  METAR_MAX_RETRIES         = 15;

Adafruit_TSL2561_Unified        tsl                       = Adafruit_TSL2561_Unified( TSL2561_ADDRESS, 1 );
uint8_t                         brightnessCurrent         = BRIGHTNESS_DEFAULT;
uint8_t                         brightnessTarget          = BRIGHTNESS_DEFAULT;
bool                            tslPresent                = false;

NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2812xMethod> ledStrip( NUM_AIRPORTS );

void setup()
{
  String mac = WiFi.macAddress();
  int pos;
  // Strip MAC address of colons
  while( (pos = mac.indexOf(':')) >= 0 ) mac.remove( pos, 1 );

  WiFi.hostname( "LED-Sectional-" + mac );

  Serial.begin( 115200 );
  delay( 5000 );

   // Fresh line
  Serial.println();
  Serial.println( "I am \"" + WiFi.hostname() + "\"" );
  
  WiFi.mode( WIFI_OFF );
  while( WiFi.status() == WL_CONNECTED ) delay(0);
  WiFi.mode( WIFI_STA );
  WiFi.begin( ssid, pass );  

  // Init onboard LED to off
  pinMode( LED_BUILTIN, OUTPUT );
  digitalWrite( LED_BUILTIN, HIGH );

  // Init airport LEDs to off
  ledStrip.Begin();
  ledStrip.Show();

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
}

void displayFlightConditions( void )
{
  for( const auto& pair : airports )
  {
    String flightCategory = pair.second.flightCategory;
    RgbColor flightCategoryColor = black;

    if( flightCategoryColors.find(flightCategory) != flightCategoryColors.end() )
    {
      if( (flightCategory == "VFR") && ((pair.second.windSpeed > WIND_THRESHOLD) || (pair.second.windGust > WIND_THRESHOLD)) )
      {
        flightCategoryColor = yellow;
      }
      else
      {
        flightCategoryColor = flightCategoryColors[flightCategory];
      }
    }
    else
    {
      Serial.println( "Unable to find color for flight category " + flightCategory );
    }

    ledStrip.SetPixelColor( pair.second.pixel, flightCategoryColor.Dim(brightnessCurrent) );
  }

  ledStrip.Show();
}

bool getMetars( void )
{
  WiFiClientSecure client;
  HTTPClient httpClient;

  client.setInsecure();

  String url = String( "https://" ) + AW_SERVER + "/" + BASE_URI;

  for( auto it = airports.begin(); it != airports.end(); ++it )
  {
    // Reset flight category
    it->second.flightCategory = "";
    it->second.lightning = false;
    it->second.windSpeed = 0;
    it->second.windGust = 0;

    // Build up URL
    if( it != airports.begin() )
      url = url + ",";

    url = url + it->first;
  }

#ifdef SECTIONAL_DEBUG
  Serial.println( url );
#endif

  Serial.print( "Starting connection to server..." );
  if( httpClient.begin(client, url) )
  {
    Serial.println( "OK" );
  }
  else
  {
    Serial.println( "Connection failed!" );
    return false;
  }  

  int16_t responseCode = httpClient.GET();

  if( responseCode != HTTP_CODE_OK )
  {
    Serial.println( "Error fetching METARs" );
    return false;
  }

  // fast forward to the array of airport results
  client.find( "\"features\":[" );

  JsonDocument doc;
  JsonDocument filter;

  // Filter out most of the data to not run out of heap
  filter["properties"]["id"] = true;
  filter["properties"]["fltcat"] = true;
  filter["properties"]["rawOb"] = true;
  filter["properties"]["wspd"] = true;
  filter["properties"]["wgst"] = true;

  // Deserialize in chunks since the entire http response cannot be kept in one big buffer
  do {
    DeserializationError error = deserializeJson( doc, client, DeserializationOption::Filter(filter) );

    if( error )
    {
      Serial.print( "deserializeJson() failed: " );
      Serial.println( error.f_str() );

      return false;
    }

    String airport = doc["properties"]["id"];
    String flightCategory = doc["properties"]["fltcat"];
    String rawOb = doc["properties"]["rawOb"];
    unsigned windSpeed = doc["properties"]["wspd"];
    unsigned windGust = doc["properties"]["wgst"];

    // Set the flight category and let the LED color mapping be done elsewhere
    airports[airport].flightCategory = flightCategory;
    airports[airport].lightning = (rawOb.indexOf("TS") != -1);
    airports[airport].windSpeed = windSpeed;
    airports[airport].windGust = windGust;

#ifdef SECTIONAL_DEBUG
    Serial.println( airport + " " + flightCategory );
    Serial.println( airport + " " + flightCategory + " " + windSpeed + "G" + windGust );
#endif

    yield();
  } while( client.findUntil(",", "]") );

  return true;
}

void loop()
{
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
      ledStrip.ClearTo( black );
      ledStrip.Show();

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

      WiFi.forceSleepWake();
      WiFi.enableSTA( true );
      WiFi.mode( WIFI_STA );
      WiFi.begin();
    }

    if( sleeping )
    {
      delay( 60 * 1000 );
      return;
    }
  }
  while( 0 );

  // Wi-Fi routine
  {
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

      // Show Wi-Fi is not connected with Orange across the board
      ledStrip.ClearTo( orange );
      ledStrip.Show();

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

      // Show success with Purple across the board
      ledStrip.ClearTo( purple );
      ledStrip.Show();
    }
  }

  // TSL2561 sensor routine
  {
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

          if( result <= UINT8_MAX )
          {
            brightnessTarget = (uint8_t) result;
          }
          else
          {
            brightnessTarget = UINT8_MAX;
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
      int stepSize = 4;

      int nextStep = brightnessCurrent;

      if( brightnessCurrent < brightnessTarget )
      {
        nextStep += stepSize;

        if( nextStep > brightnessTarget ) nextStep = brightnessTarget;
      }
      else
      {
        nextStep -= stepSize;

        if( nextStep < brightnessTarget ) nextStep = brightnessTarget;
      }

      brightnessCurrent = (uint8_t) nextStep;
      
      displayFlightConditions();
    }
  }

  // METAR routine
  {
    typedef enum {
      METAR_STATE_INIT,
      METAR_STATE_FETCHING,
      METAR_STATE_REST,
    } eMetarState;

    static unsigned long  metarLast         = 0;
    static eMetarState    metarState        = METAR_STATE_INIT;
    static unsigned long  metarInterval     = METAR_REQUEST_INTERVAL_S * 1000;
    static unsigned       metarRetryCount;

    switch( metarState )
    {
      case METAR_STATE_INIT:
        metarState = METAR_STATE_FETCHING;
        metarRetryCount = 0;
        break;

      case METAR_STATE_REST:
        if( (millis() - metarLast) < metarInterval )
        {
          break;
        }
        // Intentional fall-through

      case METAR_STATE_FETCHING:
        Serial.println( "Getting METARs" );

        metarInterval = METAR_REQUEST_INTERVAL_S * 1000;

        if( getMetars() )
        {
          metarRetryCount = 0;
        }
        else
        {
          if( ++metarRetryCount >= METAR_MAX_RETRIES )
          {
            Serial.println( "Unable" );
            metarRetryCount = 0;
          }
          else
          {
            metarInterval = METAR_RETRY_INTERVAL_S * 1000;
          }
        }

        displayFlightConditions();

        Serial.print( "METAR request again in " );
        Serial.print( metarInterval );
        Serial.println( "ms." );

        metarLast = millis();
        metarState = METAR_STATE_REST;
        break;

      default:
        metarState = METAR_STATE_INIT;
        break;
    }
  }

  // Lightning routine
  {
    static unsigned long lightningLast = 0;

    bool haveLightning = false;

    if( (LIGHTNING_INTERVAL > 0) && (millis() - lightningLast > (LIGHTNING_INTERVAL*1000)) )
    {
      lightningLast = millis();

      for( const auto& pair : airports )
      {
        if( pair.second.lightning )
        {
          // Override pixel color with white directly
          ledStrip.SetPixelColor( pair.second.pixel, white.Dim(brightnessCurrent) );
          haveLightning = true;
        }
      }

      if( haveLightning )
      {
        ledStrip.Show();
        delay( 25 );
        displayFlightConditions();
      }
    }
  }

  // All done.  Yeild to other processes.
  delay( 1000 );
}
