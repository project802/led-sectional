/**
 *  ESP8266 (NodeMCU) LED sectional based on the Arduino framework.
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */

#include "led-sectional.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <ArduinoJson.h>
#include <NeoPixelBus.h>
#include <StreamUtils.h>
#include "WorldTimeAPI.h"

#ifndef AW_SERVER
  #define AW_SERVER             "aviationweather.gov"
#endif

#ifndef BASE_URI
  #define BASE_URI              "api/data/metar?format=geojson&taf=false&ids="
#endif

const unsigned                  MAX_AIRPORTS_PER_REQUEST  = 8;
const unsigned                  METAR_READ_TIMEOUT_S      = 5;
const unsigned                  METAR_RETRY_INTERVAL_S    = 5;
const unsigned                  METAR_REQUEST_INTERVAL_S  = (15*60);
const unsigned                  METAR_MAX_RETRIES         = 2;

Adafruit_TSL2561_Unified        tsl                       = Adafruit_TSL2561_Unified( TSL2561_ADDRESS, 1 );
uint8_t                         brightnessCurrent         = BRIGHTNESS_DEFAULT;
uint8_t                         brightnessTarget          = BRIGHTNESS_DEFAULT;
bool                            tslPresent                = false;

NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2812xMethod> *ledStrip = NULL;

void setup()
{
  String mac = WiFi.macAddress();

  // Strip MAC address of colons
  mac.replace( ":", "" );

  WiFi.hostname( "LED-Sectional-" + mac );

  Serial.begin( 115200 );
  delay( 5000 ); // wait a bit here for things to settle in case we are using a serial port monitor

  // Fresh line
  Serial.println();
  Serial.println( "I am \"" + WiFi.hostname() + "\"" );
  
  WiFi.mode( WIFI_OFF );
  while( WiFi.status() == WL_CONNECTED ) delay(0);
  WiFi.mode( WIFI_STA );
  WiFi.begin( WIFI_SSID, WIFI_PASS );

  // Init onboard LED to off
  pinMode( LED_BUILTIN, OUTPUT );
  digitalWrite( LED_BUILTIN, HIGH );

  // Init airport LEDs to off
  ledStrip = new NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2812xMethod>( airports.size() );
  ledStrip->Begin();
  ledStrip->Show();

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
    const String& flightCategory = pair.second.flightCategory;
    RgbColor flightCategoryColor = black;

#ifdef SECTIONAL_DEBUG
    Serial.print( pair.second.pixel );
    Serial.print( ":" + pair.first + " " + pair.second.flightCategory + " " + pair.second.windSpeed + "G" + pair.second.windGust + " " );
    Serial.println( pair.second.lightning ? "TS" : "" );
#endif

    const auto& categoryColor = flightCategoryColors.find( flightCategory );

    if( categoryColor != flightCategoryColors.end() )
    {
      if( (flightCategory == "VFR") && ((pair.second.windSpeed > WIND_THRESHOLD) || (pair.second.windGust > WIND_THRESHOLD)) )
      {
        flightCategoryColor = yellow;
      }
      else
      {
        flightCategoryColor = categoryColor->second;
      }
    }
    else
    {
      Serial.println( "Unable to find color for flight category " + flightCategory + " for " + pair.first );
    }

    ledStrip->SetPixelColor( pair.second.pixel, flightCategoryColor.Dim(brightnessCurrent) );
  }

  ledStrip->Show();
}

unsigned getAirports( String url )
{
  unsigned foundAirports = 0;

  WiFiClientSecure client;
  HTTPClient httpClient;

  client.setInsecure();

#ifdef SECTIONAL_DEBUG
  Serial.println( "Fetching " + url );
  Serial.print( "Starting connection to server (" + String(numAirports) + " airports)..." );
#endif

  if( httpClient.begin(client, url) )
  {
#ifdef SECTIONAL_DEBUG
    Serial.println( "OK" );
#endif
  }
  else
  {
    Serial.println( "Connection failed!" );
    httpClient.end();
    return false;
  }

  // Ask http client to explicitly save the Transfer-Encoding header so we can check if it is chunked or not
  const char* headerKeys[] = { "Transfer-Encoding" };
  httpClient.collectHeaders( headerKeys, 1 );

  int16_t responseCode = httpClient.GET();

  if( responseCode != HTTP_CODE_OK )
  {
    Serial.print( "Error fetching METARs. HTTP code: " );
    Serial.println( responseCode );
    httpClient.end();
    return false;
  }

  Stream &rawStream = httpClient.getStream();
  ChunkDecodingStream decodedStream( rawStream );

  Stream& response = httpClient.header("Transfer-Encoding") == "chunked" ? decodedStream : rawStream;
  response.setTimeout( METAR_READ_TIMEOUT_S * 1000 );

  JsonDocument doc;
  JsonDocument filter;
  JsonArray features;

  // Filter out most of the data to not run out of heap
  filter["features"][0]["properties"]["id"] = true;
  filter["features"][0]["properties"]["fltcat"] = true;
  filter["features"][0]["properties"]["rawOb"] = true;
  filter["features"][0]["properties"]["wspd"] = true;
  filter["features"][0]["properties"]["wgst"] = true;

  DeserializationError error = deserializeJson( doc, response, DeserializationOption::Filter(filter) );

  if( error )
  {
    Serial.print( "deserializeJson() failed while parsing feature: " );
    Serial.println( error.f_str() );
    goto hangup;
  }

#ifdef SECTIONAL_DEBUG
  serializeJsonPretty( doc, Serial );
#endif

  features = doc["features"].as<JsonArray>();

  if( features.isNull() )
  {
    Serial.println( "Error: features array missing or invalid" );
    goto hangup;
  }

  for( JsonVariant featureVariant : features )
  {
    if( !featureVariant.is<JsonObject>() )
    {
      Serial.println( "Error: feature is not a JSON object" );
      goto hangup;
    }

    JsonObject feature = featureVariant.as<JsonObject>();

    String airport = feature["properties"]["id"];

    if( airports.find(airport) != airports.end() )
    {
      ++foundAirports;
      
      String flightCategory = feature["properties"]["fltcat"];
      String rawOb = feature["properties"]["rawOb"];
      unsigned windSpeed = feature["properties"]["wspd"];
      unsigned windGust = feature["properties"]["wgst"];

      // Load up the airport conditions into the map for later display processing
      airports[airport].flightCategory = flightCategory;
      airports[airport].lightning = (rawOb.indexOf("TS") != -1);
      airports[airport].windSpeed = windSpeed;
      airports[airport].windGust = windGust;
    }

    yield();
  }

hangup:
  httpClient.end();

#ifdef SECTIONAL_DEBUG
  Serial.println( "Found " + String(foundAirports) );
  Serial.println();
#endif

  return foundAirports;
}

bool getAllMetars( void )
{
  // Allow an error to continue. Sometimes airports don't return a METAR
  // but don't let that stop all others from reporting.

  bool retVal = true;

  unsigned numAirports = 0;

  String url;
  url.reserve( 128 );
  url = "";

  auto fetchAirportsWithRetry = [&]( String& requestUrl, unsigned requestCount ) -> bool
  {
    for( unsigned attempt = 0; attempt < 3; ++attempt )
    {
      if( requestCount == getAirports(requestUrl) )
      {
        return true;
      }
      else
      {
        Serial.println( "Retrying METAR request..." );
        delay( METAR_RETRY_INTERVAL_S * 1000 );
      }
    }

    return false;
  };

  for( auto it = airports.begin(); (it != airports.end()); ++it )
  {
    // Reset flight category
    it->second.flightCategory = "";
    it->second.lightning = false;
    it->second.windSpeed = 0;
    it->second.windGust = 0;

    if( numAirports == 0 )
    {
      url.clear();
      url.concat( "https://" );
      url.concat( AW_SERVER );
      url.concat( "/" );
      url.concat( BASE_URI );
      url.concat( it->first );
      numAirports = 1;
    }
    else
    {
      url += "," + it->first;
      numAirports++;
    }

    if( numAirports >= MAX_AIRPORTS_PER_REQUEST )
    {
      retVal = retVal && fetchAirportsWithRetry( url, numAirports );
      numAirports = 0;
    }
  }

  if( numAirports > 0 )
  {
    retVal = retVal && fetchAirportsWithRetry( url, numAirports );
  }

  return retVal;
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

    bool dayIsHoliday = (std::find(holidays.begin(), holidays.end(), wtAPI.getDayOfYear()) != holidays.end());
    
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
      ledStrip->ClearTo( black );
      ledStrip->Show();

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
        WiFi.begin( WIFI_SSID, WIFI_PASS );
      }
      
      Serial.print( "Connecting to SSID \"" );
      Serial.print( WiFi.SSID() );
      Serial.print( "\"..." );

      // Show Wi-Fi is not connected with Orange across the board
      ledStrip->ClearTo( orange );
      ledStrip->Show();

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
        WiFi.begin( WIFI_SSID, WIFI_PASS );
        return;
      }
      
      Serial.println( "OK!" );

      // Show success with Purple across the board
      ledStrip->ClearTo( purple );
      ledStrip->Show();
    }
  }

  // TSL2561 sensor routine
  {
    static unsigned long tslLast = 0;
    if( tslPresent && (millis() - tslLast > 5000) )
    {
      sensors_event_t event;

      tsl.getEvent( &event );
    
      for( unsigned i = 0; luxMap[i] != nullptr; i++ )
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
          Serial.print( ", result " );
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
      // Step size is empirically determined to be a good balance between speed and smoothness of brightness change.
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

        if( getAllMetars() )
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

        // Display what we have, regardless. Sometimes stations just don't have METARs returned.
        if( metarRetryCount == 0 )
        {
          displayFlightConditions();
        }

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

    if( (LIGHTNING_INTERVAL > 0) && (millis() - lightningLast > (LIGHTNING_INTERVAL*1000)) )
    {
      bool haveLightning = false;

      lightningLast = millis();

      for( const auto& pair : airports )
      {
        if( pair.second.lightning )
        {
          // Override pixel color with white directly
          uint8_t ratio = min( UINT8_MAX, brightnessCurrent * 2 );
          ledStrip->SetPixelColor( pair.second.pixel, white.Dim(ratio) );
          haveLightning = true;
        }
      }

      if( haveLightning )
      {
        ledStrip->Show();
        delay( 25 );
        displayFlightConditions();
      }
    }
  }

  // All done.  Yield to other processes.
  delay( 1000 );
}
