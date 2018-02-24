/*
 * AMIS30543StepperDriver.cpp
 *
 *  Created on: 2018��2��8��
 *      Author: caoyuan9642
 */

#include <AMIS30543StepperDriver.h>
#include "mbed.h"
#include "pinmap.h"
AMIS30543StepperDriver::AMIS30543StepperDriver(SPI *spi, PinName cs,
		PinName step, PinName dir, PinName err) :
		spi(spi), cs(cs, 1), step(step), dir(dir), err(err), status(IDLE), inc(
				1), stepCount(0)
{
	if (dir != NC)
	{
		useDIR = true;
		this->dir = 0; /*Forward*/
	}
	else
		useDIR = false;
	if (err != NC)
	{
		useERR = true;
	}
	else
		useERR = false;

	spi->frequency(400000);

	/*CPOL=0, CPHA=0*/
	spi->format(8, 0);

	wait_us(10);

	/*Perform initialization*/
	writeReg(CR0, 0x00); // 1/32 step
	writeReg(CR1, 0x03); // PWM double freq, V-slope = 35V/us
	writeReg(CR2, 0x80); // MOTEN = 1
	writeReg(CR3, 0x00); // 1/32 step
}

void AMIS30543StepperDriver::writeReg(regaddr_t addr, uint8_t data)
{
	assertCS();
	wait_us(3);
	txbuf[0] = (char) (addr & 0x1F) | 0x80;
	txbuf[1] = data;
	spi->write(txbuf, 2, NULL, 0);
	wait_us(3);
	deassertCS();
	wait_us(6);
}

uint8_t AMIS30543StepperDriver::readReg(regaddr_t addr)
{
	assertCS();
	wait_us(3);
	txbuf[0] = (char) (addr & 0x1F);
	spi->write(txbuf, 2, rxbuf, 2);
	wait_us(3);
	deassertCS();
	wait_us(6);
	return rxbuf[1];
}

void AMIS30543StepperDriver::start(stepdir_t dir)
{
	/*Set Direction*/
	if (status == IDLE)
	{
		if (useDIR)
		{
			if (dir == STEP_FORWARD)
				this->dir = 0;
			else
				this->dir = 1;
		}
		else
		{
			uint8_t cr1 = readReg(CR1) & 0x7F;
			if (dir == STEP_FORWARD)
				cr1 |= 0x00;
			else
				cr1 |= 0x80;
			writeReg(CR1, cr1);
		}

		inc = (dir == StepperMotor::STEP_FORWARD) ? 1 : -1;

		step.start();
		step.resetCount();
		status = STEPPING;
	}
}

void AMIS30543StepperDriver::stop()
{
	if (status == STEPPING)
	{
		status = IDLE;
		step.stop();
		stepCount += step.getCount() * inc;
	}
}

float AMIS30543StepperDriver::setPeriod(float period)
{
	if (period > 0)
	{
		this->period = step.setPeriod(period);
		return this->period;
	}
	else
		return 0;
}

int64_t AMIS30543StepperDriver::getStepCount()
{
	if (status == IDLE)
		return stepCount;
	else
		return stepCount + step.getCount() * inc;
}

void xprintf(const char *,...);
void AMIS30543StepperDriver::setMicroStep(uint8_t microstep)
{
	uint8_t cr0 = readReg(CR0) & ~(0xE0);
		
	switch (microstep)
	{
	case 1:
		writeReg(CR3, 0x03);	// Compensated full-step
		writeReg(CR0, cr0 | (0x00 << 5));  // 1/32-step
		break;
	case 2:
		writeReg(CR3, 0x00);
		writeReg(CR0, cr0 | (0x04 << 5));  // Compensated half-step
		break;
	case 4:
		writeReg(CR3, 0x00);
		writeReg(CR0, cr0 | (0x03 << 5));  // 1/4-step
		break;
	case 8:
		writeReg(CR3, 0x00);
		writeReg(CR0, cr0 | (0x02 << 5));  // 1/8-step
		break;
	case 16:
		writeReg(CR3, 0x00);
		writeReg(CR0, cr0 | (0x01 << 5));  // 1/16-step
		break;
	case 32:
		writeReg(CR3, 0x00);
		writeReg(CR0, cr0 | (0x00 << 5));  // 1/32-step
		break;
	case 64:
		writeReg(CR3, 0x02); // 1/64-step
		writeReg(CR0, cr0 | (0x00 << 5));  // 1/32-step
		break;
	case 128:
		writeReg(CR3, 0x01);
		writeReg(CR0, cr0 | (0x00 << 5));  // 1/32-step
		break;
	default:
		fprintf(stderr, "Error: microsteps must be a power of 2");
	}
}

void AMIS30543StepperDriver::setCurrent(float current)
{
	uint8_t reg_cur = 0;
	uint8_t cr0 = readReg(CR0) & ~(0x1F);
//	xprintf("CR0=%02x", readReg(CR0));
//	xprintf("CR1=%02x", readReg(CR1));
//	xprintf("CR2=%02x", readReg(CR2));
//	xprintf("CR3=%02x", readReg(CR3));
				
	if (current <= 0.132f)
	{
		reg_cur = 0;
	}
	else if (current <= 0.245f)
	{
		reg_cur = 1;
	}
	else if (current <= 0.355f)
	{
		reg_cur = 2;
	}
	else if (current <= 0.395f)
	{
		reg_cur = 3;
	}
	else if (current <= 0.445f)
	{
		reg_cur = 4;
	}
	else if (current <= 0.485f)
	{
		reg_cur = 5;
	}
	else if (current <= 0.540f)
	{
		reg_cur = 6;
	}
	else if (current <= 0.585f)
	{
		reg_cur = 7;
	}
	else if (current <= 0.640f)
	{
		reg_cur = 8;
	}
	else if (current <= 0.715f)
	{
		reg_cur = 9;
	}
	else if (current <= 0.780f)
	{
		reg_cur = 10;
	}
	else if (current <= 0.870f)
	{
		reg_cur = 11;
	}
	else if (current <= 0.955f)
	{
		reg_cur = 12;
	}
	else if (current <= 1.060f)
	{
		reg_cur = 13;
	}
	else if (current <= 1.150f)
	{
		reg_cur = 14;
	}
	else if (current <= 1.260f)
	{
		reg_cur = 15;
	}
	else if (current <= 1.405f)
	{
		reg_cur = 16;
	}
	else if (current <= 1.520f)
	{
		reg_cur = 17;
	}
	else if (current <= 1.695f)
	{
		reg_cur = 18;
	}
	else if (current <= 1.820f)
	{
		reg_cur = 19;
	}
	else if (current <= 2.070f)
	{
		reg_cur = 20;
	}
	else if (current <= 2.240f)
	{
		reg_cur = 21;
	}
	else if (current <= 2.440f)
	{
		reg_cur = 22;
	}
	else if (current <= 2.700f)
	{
		reg_cur = 23;
	}
	else if (current <= 2.845f)
	{
		reg_cur = 24;
	}
	else if (current <= 3.000f)
	{
		reg_cur = 25;
	}
	else
	{
		fprintf(stderr, "Error: maximum current supported is 3.0A/phase");
		return;
	}
	writeReg(CR0, cr0 | reg_cur);
}

inline void AMIS30543StepperDriver::assertCS()
{
	cs = 0;
}

inline void AMIS30543StepperDriver::deassertCS()
{
	cs = 1;
}


