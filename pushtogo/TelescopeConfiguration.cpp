/*
 * TelescopeConfiguration.cpp
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#include <TelescopeConfiguration.h>
#include "mbed.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

TelescopeConfiguration TelescopeConfiguration::instance =
		TelescopeConfiguration();

TelescopeConfiguration& TelescopeConfiguration::readFromFile(FILE* fp)
{
	char line[256];
	instance = TelescopeConfiguration();

	int lineno = 0;

	while (true)
	{
		if (fgets(line, sizeof(line), fp) == NULL)
			break;
		char *p = line;
		lineno++;
		// Skip any white characters in the front
		while (*p && isspace(*p))
			p++;
		if (*p == '\0')
		{
			/*Empty line*/
			continue;
		}
		// Skip commented lines
		if (*p == '#')
			continue;
		// Find the '=' sign
		char *q = strchr(p, '=');
		if (q == NULL)
		{
			/*Syntax error*/
			debug("Syntax error in line %d\n", lineno);
			continue;
		}

		/*strip the parameter name*/
		char *r = q - 1;
		while (r >= p && isspace(*r))
			r--;

		q = q + 1;
		while (*q && isspace(*q))
			q++;
		if (*q == '\0')
		{
			/*Empty value, just keep the default*/
			continue;
		}

		char *s = line + strlen(line) - 1; // Last character of the string
		while (s >= q && isspace(*s))
			s--;

		char parameter[64], value[64];
		strncpy(parameter, p, r - p + 1);
		parameter[r - p + 1] = '\0';
		strncpy(value, q, s - q + 1);
		value[s - q + 1] = '\0';

//		debug("%d: |%s|=|%s|\n", lineno, parameter, value);

		/*Iterate through possible parameters*/
		if (strcmp(parameter, "motor_steps") == 0)
		{
			instance.motor_steps = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "gear_reduction") == 0)
		{
			instance.gear_reduction = strtod(value, NULL);
		}
		if (strcmp(parameter, "worm_teeth") == 0)
		{
			instance.worm_teeth = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "ra_invert") == 0)
		{
			if (strcmp(value, "true") == 0)
				instance.ra_invert = true;
		}
		if (strcmp(parameter, "dec_invert") == 0)
		{
			if (strcmp(value, "true") == 0)
				instance.dec_invert = true;
		}
		if (strcmp(parameter, "default_slew_speed") == 0)
		{
			instance.default_slew_speed = strtod(value, NULL);
		}
		if (strcmp(parameter, "default_track_speed_sidereal") == 0)
		{
			instance.default_track_speed_sidereal = strtod(value, NULL);
		}
		if (strcmp(parameter, "default_correction_speed_sidereal") == 0)
		{
			instance.default_correction_speed_sidereal = strtod(value, NULL);
		}
		if (strcmp(parameter, "default_guide_speed_sidereal") == 0)
		{
			instance.default_guide_speed_sidereal = strtod(value, NULL);
		}
		if (strcmp(parameter, "default_acceleration") == 0)
		{
			instance.default_acceleration = strtod(value, NULL);
		}
		if (strcmp(parameter, "max_speed") == 0)
		{
			instance.max_speed = strtod(value, NULL);
		}
		if (strcmp(parameter, "min_slew_angle") == 0)
		{
			instance.min_slew_angle = strtod(value, NULL);
		}
		if (strcmp(parameter, "correction_tolerance") == 0)
		{
			instance.correction_tolerance = strtod(value, NULL);
		}
		if (strcmp(parameter, "min_correction_time") == 0)
		{
			instance.min_correction_time = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "max_correction_angle") == 0)
		{
			instance.max_correction_angle = strtod(value, NULL);
		}
		if (strcmp(parameter, "max_guide_time") == 0)
		{
			instance.max_guide_time = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "acceleration_step_time") == 0)
		{
			instance.acceleration_step_time = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "microstep_slew") == 0)
		{
			instance.microstep_slew = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "current_slew") == 0)
		{
			instance.current_slew = strtod(value, NULL);
		}
		if (strcmp(parameter, "microstep_track") == 0)
		{
			instance.microstep_track = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "current_track") == 0)
		{
			instance.current_track = strtod(value, NULL);
		}
		if (strcmp(parameter, "microstep_correction") == 0)
		{
			instance.microstep_correction = strtol(value, NULL, 0);
		}
		if (strcmp(parameter, "current_correction") == 0)
		{
			instance.current_correction = strtod(value, NULL);
		}
		if (strcmp(parameter, "current_idle") == 0)
		{
			instance.current_idle = strtod(value, NULL);
		}
	}

	return instance;
}
