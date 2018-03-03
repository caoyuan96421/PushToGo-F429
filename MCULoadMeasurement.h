#ifndef MCULOADMEASUREMENT_H_
#define MCULOADMEASUREMENT_H_

#include "mbed.h"

class MCULoadMeasurement
{
protected:
	Timer t;
	Timer t_active;
public:

	MCULoadMeasurement()
	{
		t.start();
		t_active.start();
	}
	virtual ~MCULoadMeasurement()
	{
	}

	virtual void reset()
	{
		t.reset();
		t_active.reset();
	}

	virtual float getCPUUsage()
	{
		return t_active.read() / t.read();
	}

	virtual void setMCUActive(bool active)
	{
		if (active) //idle task sched out
		{
			t_active.start();
		}
		else //idle task sched in
		{
			t_active.stop();
		}
	}

	static MCULoadMeasurement &getInstance()
	{
		static MCULoadMeasurement m;
		return m;
	}
};

#endif // MCULOADMEASUREMENT_H_
