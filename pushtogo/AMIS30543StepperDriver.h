/*
 * AMIS30543StepperDriver.h
 *
 *  Created on: 2018Äê2ÔÂ8ÈÕ
 *      Author: caoyuan9642
 */

#ifndef TELESCOPE_DRIVER_AMIS30543STEPPERDRIVER_H_

#define TELESCOPE_DRIVER_AMIS30543STEPPERDRIVER_H_

#include <StepperMotor.h>
#include <StepOut.h>
#include "mbed.h"

class AMIS30543StepperDriver: public StepperMotor
{

public:
	AMIS30543StepperDriver(SPI *spi, PinName cs, PinName step, PinName dir = NC,
			PinName err = NC);
	virtual ~AMIS30543StepperDriver()
	{
	}

	void start(stepdir_t dir);

	void stop();

	float setPeriod(float period);

	int64_t getStepCount();

	void setStepCount(int64_t set)
	{
		stepCount = set;
	}

	void setMicroStep(uint8_t microstep);

	void setCurrent(float current);

protected:
	typedef enum
	{
		WR = 0,
		CR0 = 1,
		CR1 = 2,
		CR2 = 3,
		SR0 = 4,
		SR1 = 5,
		SR2 = 6,
		SR3 = 7,
		SR4 = 8,
		CR3 = 9
	} regaddr_t;

	typedef enum
	{
		IDLE = 0, STEPPING
	} stepstatus_t;

	SPI *spi;
	DigitalOut cs;
	StepOut step;
	DigitalOut dir;
	DigitalIn err;
	volatile float period;
	volatile stepstatus_t status;
	volatile int inc;
	volatile int64_t stepCount;

	bool useDIR, useERR;

	char txbuf[2], rxbuf[2];

	void assertCS();
	void deassertCS();
	void writeReg(regaddr_t addr, uint8_t data);
	uint8_t readReg(regaddr_t addr);
};

#endif /* TELESCOPE_DRIVER_AMIS30543STEPPERDRIVER_H_ */

