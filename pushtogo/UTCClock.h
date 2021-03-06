#include <time.h>

#ifndef UTCCLOCK_H_
#define UTCCLOCK_H_
/**
 * This class provides an interface to a general clock that provides information about the current time.
 * In most systems, this class is implemented by RTCClock to read time from the RTC.
 */
class UTCClock
{
public:
	UTCClock()
	{
	}
	virtual ~UTCClock()
	{
	}

	virtual time_t getTime() = 0;
	virtual void setTime(time_t time) = 0;

};

#endif /*UTCCLOCK_H_*/
