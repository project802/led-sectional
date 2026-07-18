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

const unsigned                  MAX_AIRPORTS_PER_REQUEST    = 8;
const unsigned                  METAR_STREAM_READ_TIMEOUT_S = 5;
const unsigned                  METAR_REQUEST_INTERVAL_S    = (15*60);
const unsigned                  METAR_FETCH_TIMEOUT_S       = 60;
const unsigned                  AIRPORT_MAX_API_ATTEMPTS    = 5;

Adafruit_TSL2561_Unified        tsl                         = Adafruit_TSL2561_Unified( TSL2561_ADDRESS, 1 );
uint8_t                         brightnessCurrent           = BRIGHTNESS_DEFAULT;
uint8_t                         brightnessTarget            = BRIGHTNESS_DEFAULT;
bool                            tslPresent                  = false;
bool                            refreshMetars               = false;

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
    Serial.println( F("Unable to find TSL2561 lux sensor.  Check sensor address.") );
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
    RgbColor pixelColor = black;
    const String& flightCategory = pair.second.flightCategory;
    const auto& categoryColor = flightCategoryColors.find( flightCategory );

    if( pair.second.valid && (categoryColor != flightCategoryColors.end()) )
    {
      unsigned winds = max( pair.second.windSpeed, pair.second.windGust );

      // default to flight category color
      pixelColor = categoryColor->second;

      // override to yellow if VFR and wind/gusts exceed threshold
      if( (flightCategory == "VFR") && (winds > WIND_THRESHOLD) )
      {
        pixelColor = yellow;
      }
    }

    ledStrip->SetPixelColor( pair.second.pixel, pixelColor.Dim(brightnessCurrent) );
  }

  ledStrip->Show();
}

unsigned callAndParseMetarApi( const std::vector<String>& airportRequestList )
{
  unsigned foundAirports = 0;

  if( airportRequestList.size() == 0 )
  {
    Serial.println( F("Error: callAndParseMetarApi called with empty airportRequestList") );
    return foundAirports;
  }

  String url;
  url.reserve( 128 );
  url = "https://" + String(AW_SERVER) + "/" + String(BASE_URI);

  for( const auto& airport : airportRequestList )
  {
    url += airport + ",";
  }

  // Remove trailing comma
  url.remove( url.length() - 1 );

  WiFiClientSecure client;
  HTTPClient httpClient;

  client.setInsecure();

  if( !httpClient.begin(client, url) )
  {
    Serial.println( F("Connection failed!") );
    return foundAirports;
  }

  // Ask http client to explicitly save the Transfer-Encoding header so we can check if it is chunked or not
  const char* headerKeys[] = { "Transfer-Encoding" };
  httpClient.collectHeaders( headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]) );

  int16_t responseCode = httpClient.GET();

  if( responseCode != HTTP_CODE_OK )
  {
    Serial.print( F("Error fetching METARs. HTTP code: ") );
    Serial.println( responseCode );
    return foundAirports;
  }

  Stream &rawStream = httpClient.getStream();
  ChunkDecodingStream decodedStream( rawStream );

  Stream& response = httpClient.header("Transfer-Encoding").equalsIgnoreCase( "chunked" ) ? decodedStream : rawStream;
  response.setTimeout( METAR_STREAM_READ_TIMEOUT_S * 1000 );

  JsonDocument filter;
  
  // Filter out most of the data to not run out of heap
  filter["features"][0]["properties"]["id"] = true;
  filter["features"][0]["properties"]["fltcat"] = true;
  filter["features"][0]["properties"]["rawOb"] = true;
  filter["features"][0]["properties"]["wspd"] = true;
  filter["features"][0]["properties"]["wgst"] = true;

  JsonDocument doc;
  DeserializationError error = deserializeJson( doc, response, DeserializationOption::Filter(filter) );

  if( error )
  {
    Serial.print( F("deserializeJson() failed while parsing feature: ") );
    Serial.println( error.f_str() );
    return foundAirports;
  }

  JsonArray features = doc["features"].as<JsonArray>();

  if( features.isNull() )
  {
    Serial.println( F("Error: features array missing or invalid") );
    return foundAirports;
  }

  for( JsonVariant featureVariant : features )
  {
    if( !featureVariant.is<JsonObject>() )
    {
      Serial.println( F("Error: feature is not a JSON object") );
      return foundAirports;
    }

    JsonObject feature = featureVariant.as<JsonObject>();

    String airportId = feature["properties"]["id"];

    const auto& airportIt = airports.find(airportId);
    if( airportIt != airports.end() )
    {
      String flightCategory = feature["properties"]["fltcat"];

      if( flightCategoryColors.find( flightCategory ) == flightCategoryColors.end() )
      {
        Serial.print( F("API error: flight category is missing or invalid for ") );
        Serial.println( airportId );
        continue;
      }

      String rawOb = feature["properties"]["rawOb"];
      unsigned windSpeed = feature["properties"]["wspd"];
      unsigned windGust = feature["properties"]["wgst"];

      // Load up the airport conditions into the map for later display processing
      airportIt->second.flightCategory = flightCategory;
      airportIt->second.lightning = (rawOb.indexOf("TS") != -1);
      airportIt->second.windSpeed = windSpeed;
      airportIt->second.windGust = windGust;
      airportIt->second.valid = true;

      ++foundAirports;
    }

    yield();
  }

  if( foundAirports == 0 )
  {
    Serial.print( F("Warning: No airports were found in the METAR response for ") );

    for( const auto& airport : airportRequestList )
    {
      Serial.print( airport );
      Serial.print( " " );
    }

    Serial.println();
  }

  return foundAirports;
}

void getAllMetars( void )
{
  // Clear out all airport METAR data and mark as invalid
  for( auto it = airports.begin(); (it != airports.end()); ++it )
  {
    it->second.valid = false;
    it->second.attempts = 0;
    it->second.flightCategory = "";
    it->second.lightning = false;
    it->second.windSpeed = 0;
    it->second.windGust = 0;
  }

  bool allValid = false;
  unsigned long loopStart = millis();

  do
  {
    std::vector<String> airportList;
    airportList.reserve( MAX_AIRPORTS_PER_REQUEST );

    allValid = false;

    // Pull the first MAX_AIRPORTS_PER_REQUEST airports that are not valid and build a request URL for them
    for( auto it = airports.begin(); (it != airports.end()) && (airportList.size() < MAX_AIRPORTS_PER_REQUEST); ++it )
    {
      if( it->second.valid || (it->second.attempts >= AIRPORT_MAX_API_ATTEMPTS) )
      {
        continue;
      }

      airportList.push_back(it->first);
      ++it->second.attempts;
    }

    allValid = (airportList.size() == 0);

    if( !allValid )
    {
      unsigned numFetched = callAndParseMetarApi( airportList );

      if( numFetched != airportList.size() )
      {
        // Delay because something didn't come back. Give the API time to rest.
        delay( 3000 );
      }
    }
  }
  while( !allValid && ((millis() - loopStart) < (METAR_FETCH_TIMEOUT_S * 1000)) );

  if( !allValid )
  {
    Serial.print( F("Warning: Not all airports were valid after ") );
    Serial.print( String(METAR_FETCH_TIMEOUT_S) );
    Serial.println( F(" seconds of retries.") );
  }
}

void loop()
{
  // Update brightness target (used as input to sleep and airport pixel brightness)
  do
  {
    static unsigned long tslLast = 0;
    if( tslPresent && (millis() - tslLast > 5000) )
    {
      sensors_event_t event;

      tsl.getEvent( &event );
    
      for( unsigned i = 0; i < sizeof(luxMap) / sizeof(luxMap[0]); i++ )
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

          break;
        }
      }
      
      tslLast = millis();
    }
  }
  while( false );

  // Sleep routine
  do
  {
    static bool sleeping = false;
    bool shouldBeAsleep = (brightnessTarget == 0);
    
    if( !sleeping && shouldBeAsleep )
    {
      Serial.println( F("Good night") );
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
      sleeping = false;
      refreshMetars = true;

      WiFi.forceSleepWake();
      WiFi.enableSTA( true );
      WiFi.mode( WIFI_STA );
      WiFi.begin();

      Serial.println( F("Good morning") );
    }

    if( sleeping )
    {
      delay( 10 * 1000 );
      return;
    }
  }
  while( false );

  // Wi-Fi routine
  do
  {
    if( WiFi.status() != WL_CONNECTED )
    {
      // By default, the ESP8266 will use the stored wifi credentials to reconnect to wifi.
      // Only use the ones programmatically assigned here if there is a failure to connect or none set

      if( WiFi.SSID().length() == 0 )
      {
        Serial.println( F("No WiFi config found.  Starting.") );
        WiFi.mode( WIFI_STA );
        WiFi.begin( WIFI_SSID, WIFI_PASS );
      }
      
      Serial.print( F("Connecting to SSID \"" ) );
      Serial.print( WiFi.SSID() );
      Serial.print( F("\"...") );

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
        Serial.println( F("Failed. Will retry...") );
        WiFi.mode( WIFI_STA );
        WiFi.begin( WIFI_SSID, WIFI_PASS );
        return;
      }
      
      Serial.println( F("OK!") );

      // Show success with Purple across the board
      ledStrip->ClearTo( purple );
      ledStrip->Show();
    }
  }
  while( false );

  // METAR routine
  do
  {
    typedef enum {
      METAR_STATE_INIT,
      METAR_STATE_FETCHING,
      METAR_STATE_REST,
    } eMetarState;

    static unsigned long  metarLast         = 0;
    static eMetarState    metarState        = METAR_STATE_INIT;
    static unsigned long  metarInterval     = METAR_REQUEST_INTERVAL_S * 1000;

    if( refreshMetars )
    {
      metarState = METAR_STATE_FETCHING;
      refreshMetars = false;
    }

    switch( metarState )
    {
      case METAR_STATE_INIT:
        metarState = METAR_STATE_FETCHING;
        break;

      case METAR_STATE_REST:
        if( (millis() - metarLast) < metarInterval )
        {
          break;
        }
        // Intentional fall-through

      case METAR_STATE_FETCHING:
        Serial.println( F("Getting METARs") );

        getAllMetars();
        displayFlightConditions();

        Serial.print( F("METAR request again in ") );
        Serial.print( metarInterval );
        Serial.println( F("ms.") );

        metarLast = millis();
        metarState = METAR_STATE_REST;
        break;

      default:
        metarState = METAR_STATE_INIT;
        break;
    }
  }
  while( false );

  // Adjust brightness routine
  do
  {
    if( brightnessCurrent != brightnessTarget )
    {
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
  while( false );

  // Lightning routine
  do
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
  while( false );

  // All done.  Yield to other processes.
  delay( 1000 );
}
