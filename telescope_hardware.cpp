/**
 * Harware setup is implemented in this file
 */

#include "telescope_hardware.h"
#include "AMIS30543StepperDriver.h"
#include "AdaptiveAxis.h"
#include "EquatorialMount.h"
#include "RTCClock.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "TelescopeConfiguration.h"

/**
 * Right-Ascenstion Axis
 */
SPI ra_spi(PC_12, PC_11, PB_3_ALT0);
AMIS30543StepperDriver ra_stepper(&ra_spi, PE_3, PB_7);

/**
 * Declination Axis
 */
SPI dec_spi(PE_6, PE_5, PE_2);
AMIS30543StepperDriver dec_stepper(&dec_spi, PE_4, PC_8);

/**
 * Clock object
 */
RTCClock clk;

/**
 * SD card reader hardware configuration
 */
SDBlockDevice sdb(PA_7, PB_4, PA_5, PC_13);
FATFileSystem fs("sdcard");

const char *config_file_path = "/sdcard/telescope.cfg";

EquatorialMount &telescopeHardwareInit()
{
	TelescopeConfiguration &telescope_config =
			TelescopeConfiguration::getDefaultConfiguration();
	// Read configuration
	printf("Mounting SD card...\n");
	if (fs.mount(&sdb) != 0)
	{
		debug(
				"Error: failed to mount SD card. Falling back to default configuration.\n");
	}
	else
	{
		FILE *fp = fopen(config_file_path, "r");
		if (fp == NULL)
		{
			debug(
					"Error: config file not accessible: %s. Falling back to default configuration\n",
					config_file_path);
		}
		else
		{
			printf("Reading configuration file %s\n", config_file_path);
			telescope_config = TelescopeConfiguration::readFromFile(fp);
			fclose(fp);
		}
	}

	// Object initialization
	AdaptiveAxis &ra_axis = *new AdaptiveAxis(telescope_config.getStepsPerDeg(),
			&ra_stepper, telescope_config, telescope_config.isRAInvert(),
			"RA_Axis");
	AdaptiveAxis &dec_axis = *new AdaptiveAxis(
			telescope_config.getStepsPerDeg(), &dec_stepper, telescope_config,
			telescope_config.isDECInvert(), "DEC_Axis");
	EquatorialMount &eq_mount = *new EquatorialMount(ra_axis, dec_axis, clk,
			telescope_config.getLocation());

	printf("Telescope initialized\n");

	return eq_mount;
}
