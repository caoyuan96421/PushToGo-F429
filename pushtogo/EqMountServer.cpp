/*
 * EqMountServer.cpp
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#include "EqMountServer.h"
#include "mbed_events.h"
#include <ctype.h>
#include "MCULoadMeasurement.h"

#define EMS_DEBUG 0
#define MAX_COMMAND 128

#define ERR_WRONG_NUM_PARAM 1
#define ERR_PARAM_OUT_OF_RANGE 2

extern ServerCommand commandlist[MAX_COMMAND];

static void stprintf(FileHandle &f, const char *fmt, ...)
{
	char buf[128];
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	f.write(buf, len);
}

EqMountServer::EqMountServer(FileHandle &stream, bool echo) :
		eq_mount(NULL), stream(stream), thread(osPriorityBelowNormal,
		OS_STACK_SIZE, NULL, "EqMountServer"), echo(echo), commandRunning(false)
{
	thread.start(callback(this, &EqMountServer::task_thread));
}

EqMountServer::~EqMountServer()
{
	thread.terminate();
}

void EqMountServer::task_thread()
{
	char buffer[256]; // text buffer
	commandRunning = false;
	EventQueue queue(4 * EVENTS_EVENT_SIZE);
	Thread evq_thd(osThreadGetPriority(Thread::gettid()), OS_STACK_SIZE, NULL,
			"EqMountServer dispatcher");
	evq_thd.start(callback(&queue, &EventQueue::dispatch_forever));

	while (true)
	{
		bool eof = false;
		char x = 0;
		int i = 0;
		while (!eof && i < (int) sizeof(buffer) - 10)
		{
			int s = stream.read(&x, 1);
			if (s <= 0)
			{ // End of file
				eof = true;
				break;
			}
			else if (x == '\r' || x == '\n')
			{ // End of line
				break;
			}
			else if (x == '\b' || x == '\x7F')
			{ // Backspace
				if (i > 0)
				{
					const char str[] = "\b \b";
					stream.write(str, sizeof(str)); // blank the current character properly
					i--;
				}
				continue;
			}
			else if (isspace(x))
			{ // Convert to white space
				x = ' ';
			}
			else if (!isprint(x))
			{ // Ignore everything else
				continue;
			}
			// Echo
			if (echo)
			{
				stream.write(&x, 1);
			}
			buffer[i++] = (char) x;
		}
		if (eof && i == 0)
			break;
		if (echo)
		{
			// Echo new line character after command
			x = '\r';
			stream.write(&x, 1);
			x = '\n';
			stream.write(&x, 1);
		}
		if (i == 0) // Empty command
			continue;
		buffer[i] = '\0'; // insert null character

		if (eq_mount == NULL)
		{
			stprintf(stream, "Error: EqMount not binded.\r\n");
			continue;
		}

		char delim[] = " "; // Delimiter, can be any white character in the actual input
		char *saveptr;

		char * command = strtok_r(buffer, delim, &saveptr); // Get the first token

		if (strlen(command) == 0) // Empty command
			continue;

		for (char *p = command; *p; ++p)
			*p = tolower(*p); // Convert to lowercase

		char *args[16];

		// Extract parameters
		i = 0;
		do
		{
			args[i] = strtok_r(NULL, delim, &saveptr);
			if (args[i] == NULL || ++i == 16)
				break;
		} while (true);

		int argn = i;

		debug_if(EMS_DEBUG, "command: |%s| ", command);
		for (i = 0; i < argn; i++)
		{
			debug_if(EMS_DEBUG, "|%s| ", args[i]);
			for (char *p = args[i]; *p; ++p)
				*p = tolower(*p); // Convert to lowercase
		}
		debug_if(EMS_DEBUG, "\n");

		int cind = -1;

		for (i = 0; i < MAX_COMMAND && commandlist[i].fptr != NULL; i++)
		{
			if (strcmp(commandlist[i].cmd, command) == 0)
			{
				cind = i;
				break;
			}
		}

		if (cind == -1)
		{
			debug_if(EMS_DEBUG, "Error: command %s not found.\n", command);
			continue;
		}

		if (commandRunning)
		{
			// Already running, only accepts stop/emergStop
			if (cind < 2)
				commandlist[cind].fptr(this, 0, NULL);
			else
				continue;
		}

		commandRunning = true;
		Callback<void(ServerCommand&, int, char**)> cb = callback(this,
				&EqMountServer::command_execute);
		queue.call(cb, commandlist[cind], argn, args); // Use the event dispatching thread to run this

	}
	// If we reach here, it must be end of file
}

void EqMountServer::command_execute(ServerCommand &cmd, int argn, char *argv[])
{
	int ret = cmd.fptr(this, argn, argv);

	if (ret == ERR_WRONG_NUM_PARAM)
	{
		// Wrong number of arguments
		debug_if(EMS_DEBUG, "Error: %s wrong number of args.\n", cmd.cmd);
	}
	else if (ret == 2)
	{
		// Wrong number of arguments
		debug_if(EMS_DEBUG, "Error: %s parameters out of range.\n", cmd.cmd);
	}
	else if (ret)
	{
		debug_if(EMS_DEBUG, "Error: %s returned code %d.\n", cmd.cmd, ret);
	}

	// Send the return status back
	stprintf(stream, "%s %d\r\n", cmd.cmd, ret);

	commandRunning = false;
}

static int eqmount_stop(EqMountServer *server, int argn, char *argv[])
{
	if (argn != 0)
	{
		return ERR_WRONG_NUM_PARAM;
	}
	server->getEqMount()->stopWait();
	return 0;
}

static int eqmount_estop(EqMountServer *server, int argn, char *argv[])
{
	server->getEqMount()->emergencyStop();
	return 0;
}

static int eqmount_goto(EqMountServer *server, int argn, char *argv[])
{
	if (argn != 2)
	{ // Must be two args
		return ERR_WRONG_NUM_PARAM;
	}
	char *tp;
	double ra = strtod(argv[0], &tp);
	if (tp == argv[0])
	{
		return ERR_PARAM_OUT_OF_RANGE;
	}
	double dec = strtod(argv[1], &tp);
	if (tp == argv[1])
	{
		return ERR_PARAM_OUT_OF_RANGE;
	}

	if (!((ra <= 180.0) && (ra >= -180.0) && (dec <= 90.0) && (dec >= -90.0)))
		return ERR_PARAM_OUT_OF_RANGE;

	osStatus s;
	if ((s = server->getEqMount()->goTo(ra, dec)) != osOK)
		return s;

	return 0;
}

static int eqmount_nudge(EqMountServer *server, int argn, char *argv[])
{
	if (argn != 1 && argn != 2)
	{
		return ERR_WRONG_NUM_PARAM;
	}
	int dir = 0;
	for (int i = 0; i < argn; i++)
	{
		if (strcmp(argv[i], "south") == 0)
			dir |= NUDGE_SOUTH;
		else if (strcmp(argv[i], "north") == 0)
			dir |= NUDGE_NORTH;
		else if (strcmp(argv[i], "east") == 0)
			dir |= NUDGE_EAST;
		else if (strcmp(argv[i], "west") == 0)
			dir |= NUDGE_WEST;
		else if (strcmp(argv[i], "stop") == 0)
		{
			// STOP nudging
			dir = NUDGE_NONE;
			break;
		}
		else
		{
			return ERR_PARAM_OUT_OF_RANGE;
		}
	}

	osStatus s;
	if ((s = server->getEqMount()->startNudge((nudgedir_t) dir)) != osOK)
		return s;

	return 0;
}

static int eqmount_track(EqMountServer *server, int argn, char *argv[])
{
	if (argn != 0)
	{
		return ERR_WRONG_NUM_PARAM;
	}
	osStatus s;
	if ((s = server->getEqMount()->startTracking()) != osOK)
		return s;
	return 0;
}

static int eqmount_readpos(EqMountServer *server, int argn, char *argv[])
{
	if (argn != 0)
	{
		return ERR_WRONG_NUM_PARAM;
	}

	EquatorialCoordinates eq = server->getEqMount()->getEquatorialCoordinates();
	stprintf(server->getStream(), "%.6f %.6f\r\n", eq.ra, eq.dec);

	return 0;
}

static int eqmount_help(EqMountServer *server, int argn, char *argv[])
{
	const char *str = "Available commands: \r\n";
	server->getStream().write(str, strlen(str));
	for (int i = 0; i < MAX_COMMAND; i++)
	{
		if (commandlist[i].fptr == NULL)
			break;
		stprintf(server->getStream(), "- %s : %s\r\n", commandlist[i].cmd,
				commandlist[i].desc);
	}
	return 0;
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

static int eqmount_time(EqMountServer *server, int argn, char *argv[])
{
	time_t t = time(NULL);

	stprintf(server->getStream(), "Current UTC time: %s\r\n", ctime(&t));

	return 0;
}

ServerCommand commandlist[MAX_COMMAND] =
{ /// List of all commands
		ServerCommand("stop", "Stop mount motion", eqmount_stop), 		/// Stop
		ServerCommand("estop", "Emergency stop", eqmount_estop), /// Emergency Stop
		ServerCommand("goto",
				"Perform go to operation to specified ra, dec coordinates",
				eqmount_goto), 		/// Go to
		ServerCommand("nudge", "Perform nudging on specified direction",
				eqmount_nudge), 		/// Nudge
		ServerCommand("track", "Start tracking in specified direction",
				eqmount_track), 		/// Track
		ServerCommand("readpos", "Read current RA/DEC position",
				eqmount_readpos), /// Read Position
		ServerCommand("help", "Print this help menu", eqmount_help), /// Help menu
		ServerCommand("sys", "Print system information", eqmount_sys), /// sys
		ServerCommand("time", "Print current time & date", eqmount_time), /// sys
		};
