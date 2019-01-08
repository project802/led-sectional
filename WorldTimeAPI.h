#pragma once

#include "Arduino.h"

class WorldTimeAPI {
  public:
    enum TimeMethod
    {
      TIME_USING_IP,
      TIME_USING_TIMEZONE
    };
    
  private:
    const char *    _serverBaseURL      = "http://worldtimeapi.org/api";
    unsigned long   _updateIntervalMs   = (24*60*60*1000);
    bool            _receivedTime       = false;
    
    long            _utcOffsetS         = 0;
    unsigned long   _timeSinceEpochS    = 0;
    unsigned long   _lastUpdateMs       = 0;

    TimeMethod      _timeMethod;
    String          _timezone;
    
  public:
    WorldTimeAPI( TimeMethod method = TIME_USING_IP, String timezone = "America/New_York" );

    bool update();
    bool timeReceived()
    {
      return this->_receivedTime;
    }
    void setUtcOffset( long offsetSeconds )
    {
      this->_utcOffsetS = offsetSeconds;
    }
    
    unsigned long getUnixTime();
    unsigned getDay();
    unsigned getHour();
    unsigned getMinute();
    unsigned getSecond();
    String getFormattedTime();
};
