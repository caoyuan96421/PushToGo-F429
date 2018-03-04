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
#include "EqMountServer.h"
#include "MCULoadMeasurement.h"

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
AdaptiveAxis *ra_axis = NULL;
AdaptiveAxis *dec_axis = NULL;
EquatorialMount *eq_mount = NULL;

static void add_sys_commands();

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

	// Object re-initialization
	if (ra_axis != NULL)
	{
		delete ra_axis;
	}
	if (dec_axis != NULL)
	{
		delete dec_axis;
	}
	if (eq_mount != NULL)
	{
		delete eq_mount;
	}
	ra_axis = new AdaptiveAxis(telescope_config.getStepsPerDeg(), &ra_stepper,
			telescope_config, telescope_config.isRAInvert(), "RA_Axis");
	dec_axis = new AdaptiveAxis(telescope_config.getStepsPerDeg(), &dec_stepper,
			telescope_config, telescope_config.isDECInvert(), "DEC_Axis");
	eq_mount = new EquatorialMount(*ra_axis, *dec_axis, clk,
			telescope_config.getLocation());

	printf("Telescope initialized\n");

	return (*eq_mount); // Return reference to eq_mount
}

/* Serial connection */
UARTSerial pc(USBTX, USBRX, 115200);

EqMountServer server_serial(pc, true);

bool serverInitialized = false;

osStatus telescopeServerInit()
{
	if (eq_mount == NULL)
		return osErrorResource;
	if (!serverInitialized)
	{
		// Only run once
		serverInitialized = true;
		add_sys_commands();
	}

	server_serial.bind(*eq_mount);

	const char *str = "Server initialized.\n";
	printf(str);
	stprintf(pc, str);

	return osOK;
}

static int eqmount_sys(EqMountServer *server, int argn, char *argv[])
{
	const int THD_MAX = 32;
	osThreadId thdlist[THD_MAX];
	int nt = osThreadEnumerate(thdlist, THD_MAX);

	stprintf(server->getStream(), "Thread list: \r\n");
	for (int i = 0; i < nt; i++)
	{
		osThreadState_t state = osThreadGetState(thdlist[i]);
		const char *s = "";
		const char *n;

		if (osThreadGetPriority(thdlist[i]) == osPriorityIdle)
		{
			n = "Idle thread";
		}
		else
		{
			n = osThreadGetName(thdlist[i]);
			if (n == NULL)
				n = "System thread";
		}

		switch (state)
		{
		case osThreadInactive:
			s = "Inactive";
			break;
		case osThreadReady:
			s = "Ready";
			break;
		case osThreadRunning:
			s = "Running";
			break;
		case osThreadBlocked:
			s = "Blocked";
			break;
		case osThreadTerminated:
			s = "Terminated";
			break;
		case osThreadError:
			s = "Error";
			break;
		default:
			s = "Unknown";
			break;
		}
		stprintf(server->getStream(), " - %10s 0x%08x %s \r\n", s,
				(uint32_t) thdlist[i], n);
	}

	stprintf(server->getStream(), "\r\nRecent CPU usage: %.1f%%\r\n",
			MCULoadMeasurement::getInstance().getCPUUsage() * 100);
	return 0;
}

static int eqmount_systime(EqMountServer *server, int argn, char *argv[])
{
	char buf[32];
	time_t t = time(NULL);
	ctime_r(&t, buf);
	stprintf(server->getStream(), "Current UTC time: %s\r\n", buf);

	return 0;
}

static void add_sys_commands()
{
	EqMountServer::addCommand(
			ServerCommand("sys", "Print system information", eqmount_sys));
	EqMountServer::addCommand(
			ServerCommand("systime", "Print system time", eqmount_systime));
}
