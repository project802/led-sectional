/**
 *  ESP8266 (NodeMCU) client library for World Time API
 *  
 *  Copyright (c) 2018 Chris Vincent
 *  
 *  For more information, licensing and instructions, see https://github.com/project802/led-sectional
 */
 
#pragma once
#ifndef WORLDTIMEAPI_H
#define WORLDTIMEAPI_H

#include "Arduino.h"

class WorldTimeAPI {
  public:
    enum TimeMethod
    {
      TIME_USING_IP,
      TIME_USING_TIMEZONE
    };
    
  private:
    const char *          _serverBaseURL      = "http://worldtimeapi.org/api";
    const unsigned long   _updateIntervalMs   = (24*60*60*1000);
    const unsigned long   _retryIntervalMs    = (60*1000);

    bool                  _receivedTime       = false;
    long                  _utcOffsetS         = 0;
    unsigned long         _timeSinceEpochS    = 0;
    unsigned long         _lastUpdateMs       = 0;
    unsigned long         _lastAttemptMs      = 0;
    unsigned long         _yearStartedUtc     = 0;

    TimeMethod            _timeMethod;
    String                _timezone;
    
  public:
    WorldTimeAPI( TimeMethod method = TIME_USING_IP, String timezone = "America/New_York" );

    bool update();   
    unsigned long getUnixTime( bool utc = true );
    unsigned getDayOfWeek();
    unsigned getHour();
    unsigned getMinute();
    unsigned getSecond();
    String getFormattedTime();
    unsigned getDayOfYear();

    /**
     * @return True if the current time has been sync'd
     */
    bool timeReceived()
    {
      return this->_receivedTime;
    }
};

#endif
