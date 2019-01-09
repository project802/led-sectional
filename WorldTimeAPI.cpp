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

WorldTimeAPI::WorldTimeAPI( TimeMethod method, String timezone )
{
  this->_timeMethod = method;
  this->_timezone = timezone;
}

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

  HTTPClient http;
  WiFiClient client;
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

  String json = http.getString();
  http.end();
  
  DynamicJsonDocument jsonDoc;
  DeserializationError error = deserializeJson( jsonDoc, json );
  if( error )
  {
    Serial.print( "Unable to deserialize JSON.  Error " );
    Serial.println( error.code() );
    return false;
  }
  
  JsonObject root = jsonDoc.as<JsonObject>();

  this->_timeSinceEpochS = root["unixtime"];

  // atoi() will stop after the hour component so if support is required for
  // non-hourly offsets then one needs to parse the second half of the offset as well
  int utc_offset = atoi( root["utc_offset"] );
  this->_utcOffsetS = utc_offset * 60 * 60;

  this->_lastUpdateMs = millis();
  this->_receivedTime = true;
  
  return true;
}

unsigned long WorldTimeAPI::getUnixTime()
{
  // UTC offset + time since epoch in UTC + elapsed time since the last update
  return this->_utcOffsetS + this->_timeSinceEpochS + ((millis() - this->_lastUpdateMs) / 1000);
}

unsigned WorldTimeAPI::getDay()
{
  return (((this->getUnixTime() / 86400L) + 4 ) % 7); //0 is Sunday
}

unsigned WorldTimeAPI::getHour()
{
  return ((this->getUnixTime() % 86400L) / 3600);
}

unsigned WorldTimeAPI::getMinute()
{
  return ((this->getUnixTime() % 3600) / 60);
}

unsigned WorldTimeAPI::getSecond()
{
  return (this->getUnixTime() % 60);
}

String WorldTimeAPI::getFormattedTime()
{
   char result[12];

   snprintf( result, sizeof(result), "%i:%02i:%02i", this->getHour(), this->getMinute(), this->getSecond() );

   return String(result);
}
