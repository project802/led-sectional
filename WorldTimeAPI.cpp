/**
 *  ESP8266 (NodeMCU) client library for World Time API
 *  
 *  Copyright (c) 2018 Chris Vincent
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */

#include "WorldTimeAPI.h"
#include <ESP8266HTTPClient.h>

WorldTimeAPI::WorldTimeAPI( TimeMethod method, String timezone )
{
  this->_timeMethod = method;
  this->_timezone = timezone;
}

bool WorldTimeAPI::update()
{
  if( (this->_lastUpdateMs != 0) && (millis() - this->_lastUpdateMs < this->_updateIntervalMs) )
  {
    return false;
  }

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

  http.begin( url );

  int httpCode = http.GET();
  if( httpCode == 0 )
  {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  this->_lastUpdateMs = millis();
  
  int unixtime = payload.indexOf( "unixtime\":\"" );
  if( unixtime < 0 )
  {
    return false;
  }
  this->_timeSinceEpochS = payload.substring(unixtime + 11).toInt();

  int utc_offset = payload.indexOf( "utc_offset\":\"" );
  if( utc_offset < 0 )
  {
    return false;
  }
  this->setUtcOffset( payload.substring(utc_offset + 13).toInt() * 60 * 60 );

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
