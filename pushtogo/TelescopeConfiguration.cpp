/*
 * TelescopeConfiguration.cpp
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#include <TelescopeConfiguration.h>

TelescopeConfiguration& TelescopeConfiguration::readFromFile(FILE* fp)
{
	TelescopeConfiguration &tc = *new TelescopeConfiguration;
	tc.current_slew = 1.5;
	tc.current_track = 0.5;
	tc.current_correction = 0.5;
	tc.current_idle = 0.3;

	//TODO
	return tc;
}
