/**
 *  ESP8266 (NodeMCU) client library for World Time API
 *  
 *  Copyright (c) 2018 Chris Vincent
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */

#include "WorldTimeAPI.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

/**
 * Construct the WorldTimeAPI class.  Does not fetch the time.
 * 
 * @param method Which method to use when asking for the current time
 * @param timezone Used when the method to fetch time is TIME_USING_TIMEZONE
 */
WorldTimeAPI::WorldTimeAPI( TimeMethod method, String timezone )
{
  this->_timeMethod = method;
  this->_timezone = timezone;
}

/**
 * Main working function of WorldTimeAPI.  Call in the loop and this will 
 * take care of updating the time automatically.
 * 
 * @return true whenever a successful update occurs, false if there is no attempt or a failure
 */
bool WorldTimeAPI::update()
{
  // When we have received a valid time, operate on update interval
  // If not, operate on the retry interval
  if( this->_receivedTime )
  {
    if( (this->_lastUpdateMs != 0) && (millis() - this->_lastUpdateMs < this->_updateIntervalMs) )
    {
      return false;
    }
  }
  else
  {
    if( (this->_lastAttemptMs != 0) && (millis() - this->_lastAttemptMs < this->_retryIntervalMs) )
    {
      return false;
    }
  }

  // Assume failure and log attempt
  this->_receivedTime = false;
  _lastAttemptMs = millis();

  WiFiClient client;
  HTTPClient http;
  String url = String( _serverBaseURL );

  switch( this->_timeMethod )
  {
    default:
    case TIME_USING_IP:
      url = url + "/ip";
      break;
      
    case TIME_USING_TIMEZONE:
      url = url + "/timezone/" + this->_timezone;
      break;
  }

  if( !http.begin(client, url) )
  {
    return false;
  }

  if( http.GET() != HTTP_CODE_OK )
  {
    http.end();
    return false;
  }

  // Capture the update time as close to the HTTP GET as possible for highest accuracy
  this->_lastUpdateMs = millis();

  String json = http.getString();
  http.end();
  
  DynamicJsonDocument jsonDoc( 1024 );
  DeserializationError error = deserializeJson( jsonDoc, json );
  if( error )
  {
    Serial.print( "WorldTimeAPI: Unable to deserialize JSON.  Error " );
    Serial.println( error.code() );
    return false;
  }
  
  JsonObject root = jsonDoc.as<JsonObject>();

  if( !root["unixtime"] || !root["utc_offset"] || !root["day_of_year"] )
  {
    Serial.println( "WorldTimeAPI: Missing data in response." );
    return false;
  }
  
  this->_timeSinceEpochS = root["unixtime"];

  // atoi() will stop after the hour component so if support is required for
  // non-hourly offsets then one needs to parse the second half of the offset as well
  int utc_offset = atoi( root["utc_offset"] );
  this->_utcOffsetS = utc_offset * 60 * 60;
  
  unsigned dayOfYear = root["day_of_year"];
  this->_yearStartedUtc = this->_timeSinceEpochS + this->_utcOffsetS - ((dayOfYear-1) * 24 * 60 * 60) - (this->getHour() * 60 * 60) - (this->getMinute() * 60) - this->getSecond();
  
  this->_receivedTime = true;
  
  return true;
}

/**
 * @return the time in seconds since the unix epoch
 */
unsigned long WorldTimeAPI::getUnixTime( bool utc )
{
  unsigned long retVal = this->_timeSinceEpochS + ((millis() - this->_lastUpdateMs) / 1000);

  if( !utc )
  {
    retVal += this->_utcOffsetS;
  }
  
  return  retVal;
}

/**
 * @return a zero-indexed day of the week with Sunday being zero, Monday one, etc.
 */
unsigned WorldTimeAPI::getDayOfWeek()
{
  return (((this->getUnixTime(false) / 86400L) + 4 ) % 7);
}

/**
 * @return The hour component, in 24 hour format, of the current time
 */
unsigned WorldTimeAPI::getHour()
{
  return ((this->getUnixTime(false) % 86400L) / 3600);
}

/**
 * @return The minute component of the current time
 */
unsigned WorldTimeAPI::getMinute()
{
  return ((this->getUnixTime(false) % 3600) / 60);
}

/**
 * @return The second component of the current time
 */
unsigned WorldTimeAPI::getSecond()
{
  return (this->getUnixTime(false) % 60);
}

/**
 * @return A string representing the current time in [H]H:MM:SS
 */
String WorldTimeAPI::getFormattedTime()
{
   char result[12];

   snprintf( result, sizeof(result), "%u:%02u:%02u", this->getHour(), this->getMinute(), this->getSecond() );

   return String(result);
}

/**
 * @return The day of the year starting at 1
 */
unsigned WorldTimeAPI::getDayOfYear()
{
  // This does not work accurately if time crosses into a new year before an update in the new year
  // Would need to compute the length of the year(s) since update, which requires tracking the year from update
  
  return (this->getUnixTime(false) - this->_yearStartedUtc) / (24 * 60 * 60) + 1;
}
