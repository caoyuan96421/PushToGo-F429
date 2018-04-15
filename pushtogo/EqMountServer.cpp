/*
 * EqMountServer.cpp
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#include "EqMountServer.h"
#include "mbed_events.h"
#include <ctype.h>

#define EMS_DEBUG 0
#define MAX_COMMAND 128

#define ERR_WRONG_NUM_PARAM 1
#define ERR_PARAM_OUT_OF_RANGE 2

extern ServerCommand commandlist[MAX_COMMAND];

void stprintf(FileHandle &f, const char *fmt, ...)
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
	if (argn == 1)
	{
		if (strcmp(argv[0], "index") == 0)
		{
			return server->getEqMount()->goToIndex();
		}
	}
	else if (argn == 2)
	{
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
	}
	else
	{
		return ERR_WRONG_NUM_PARAM;
	}

	return 0;
}

static int eqmount_speed(EqMountServer *server, int argn, char *argv[])
{
	char *tp;
	if (argn == 1)
	{
		double speed = strtod(argv[0], &tp);
		if (tp == argv[0])
		{
			return ERR_PARAM_OUT_OF_RANGE;
		}
		if (speed <= 0
				|| speed > TelescopeConfiguration::getInstance().getMaxSpeed())
		{
			return ERR_PARAM_OUT_OF_RANGE;
		}
		server->getEqMount()->setSlewSpeed(speed);
	}
	else if (argn == 2)
	{
		char *tp;
		double speed = strtod(argv[1], &tp);
		if (tp == argv[1])
		{
			return ERR_PARAM_OUT_OF_RANGE;
		}
		if (strcmp(argv[0], "slew") == 0)
		{
			if (speed <= 0
					|| speed
							> TelescopeConfiguration::getInstance().getMaxSpeed())
			{
				return ERR_PARAM_OUT_OF_RANGE;
			}
			server->getEqMount()->setSlewSpeed(speed);
		}
		else if (strcmp(argv[0], "track") == 0)
		{
			if (speed <= 0 || speed > 64)
			{
				return ERR_PARAM_OUT_OF_RANGE;
			}
			server->getEqMount()->setTrackSpeedSidereal(speed);
		}
	}
	else
	{
		return ERR_WRONG_NUM_PARAM;
	}

	return 0;
}

static int eqmount_align(EqMountServer *server, int argn, char *argv[])
{
	if (argn == 0)
	{
		stprintf(server->getStream(),
				"Usage: align add [star]\nalign del [n]\nalign show\nalign show [n]\nalign replace [n] [star]\nalign clear\n");
		return ERR_WRONG_NUM_PARAM;
	}
	if (strcmp(argv[0], "add") == 0)
	{
		if (argn != 3)
		{
			stprintf(server->getStream(),
					"Usage: align add {ref_ra} {ref_dec}\n");
			return ERR_WRONG_NUM_PARAM;
		}
		char *tp;
		double ref_ra = strtod(argv[1], &tp);
		if (tp == argv[1])
		{
			return ERR_PARAM_OUT_OF_RANGE;
		}
		double ref_dec = strtod(argv[2], &tp);
		if (tp == argv[2])
		{
			return ERR_PARAM_OUT_OF_RANGE;
		}
		AlignmentStar as;
		as.star_ref = EquatorialCoordinates(ref_dec, ref_ra);
		as.star_meas = server->getEqMount()->getMountCoordinates();
		as.timestamp = server->getEqMount()->getClock().getTime();
		server->getEqMount()->addAlignmentStar(as);
	}
	else if (strcmp(argv[0], "show") == 0)
	{
		if (argn == 1)
		{
			// Show all alignment stars
			int N = server->getEqMount()->getNumAlignmentStar();
			if (N == 0)
			{
				stprintf(server->getStream(), "No alignment star\n");
			}
			else
			{
				for (int i = 0; i < N; i++)
				{
					AlignmentStar *as = server->getEqMount()->getAlignmentStar(
							i);
					stprintf(server->getStream(),
							"Star %d\n  REF: RA=%7.2f, DEC=%7.2f\n MEAS: RA=%7.2f, DEC=%7.2f %c\n",
							i + 1, as->star_ref.ra, as->star_ref.dec,
							as->star_meas.ra_delta, as->star_meas.dec_delta,
							(as->star_meas.side == PIER_SIDE_WEST) ? 'W' : 'E');
				}
				stprintf(server->getStream(), "Offset: RA=%7.2f, DEC=%7.2f\n",
						server->getEqMount()->getCalibration().offset.ra_off,
						server->getEqMount()->getCalibration().offset.dec_off);
				stprintf(server->getStream(), "Polar: ALT=%7.2f, AZ=%7.2f\n",
						server->getEqMount()->getCalibration().pa.alt,
						server->getEqMount()->getCalibration().pa.azi);
				stprintf(server->getStream(), "Polar: cone=%6.3f\n",
						server->getEqMount()->getCalibration().cone);
			}
		}
		else if (argn == 2)
		{
			int index = strtol(argv[1], NULL, 0);
			AlignmentStar *as = server->getEqMount()->getAlignmentStar(index);
			if (!as)
			{
				return ERR_PARAM_OUT_OF_RANGE;
			}
			stprintf(server->getStream(),
					"REF: RA=%7.2f, DEC=%7.2f\n MEAS: RA=%7.2f, DEC=%7.2f %c\n",
					as->star_ref.ra, as->star_ref.dec, as->star_meas.ra_delta,
					as->star_meas.dec_delta,
					(as->star_meas.side == PIER_SIDE_WEST) ? 'W' : 'E');
		}
		else
		{
			return ERR_WRONG_NUM_PARAM;
		}
	}
	else if (strcmp(argv[0], "clear") == 0)
	{
		if (argn != 1)
		{
			return ERR_WRONG_NUM_PARAM;
		}
		server->getEqMount()->clearCalibration();
	}
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

	if (argn == 0)
	{
		EquatorialCoordinates eq =
				server->getEqMount()->getEquatorialCoordinates();
		stprintf(server->getStream(), "%8.3f %8.3f\r\n", eq.ra, eq.dec);
	}
	else if (argn == 1)
	{
		if (strcmp(argv[0], "eq") == 0)
		{
			EquatorialCoordinates eq =
					server->getEqMount()->getEquatorialCoordinates();
			stprintf(server->getStream(), "%8.3f %8.3f\r\n", eq.ra, eq.dec);
		}
		else if (strcmp(argv[0], "mount") == 0)
		{
			MountCoordinates mc = server->getEqMount()->getMountCoordinates();
			stprintf(server->getStream(), "%8.3f %8.3f %c\r\n", mc.ra_delta,
					mc.dec_delta, (mc.side == PIER_SIDE_WEST) ? 'W' : 'E');
		}
		else
		{
			return ERR_PARAM_OUT_OF_RANGE;
		}
	}
	else
	{
		return ERR_WRONG_NUM_PARAM;
	}

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

static int eqmount_time(EqMountServer *server, int argn, char *argv[])
{
	char buf[32];
	//Get time
	time_t t = server->getEqMount()->getClock().getTime();

	if (argn >= 2)
	{
		return 1;
	}
	else if (argn == 1)
	{
		if (strcmp(argv[0], "stamp") == 0)
		{
			// Print timestamp value
			stprintf(server->getStream(), "%d\r\n", t);
			return 0;
		}
		else if (strcmp(argv[0], "sidereal") == 0)
		{
			// Print sidereal time at current location
			// 0.0 is sidereal midnight, 180/-180 is sidereal noon
			double st = CelestialMath::getLocalSiderealTime(t,
					server->getEqMount()->getLocation());
			int hh = ((int) floor(st / 15) + 24) % 24;
			int mm = (int) floor((st + 360.0 - hh * 15) * 4) % 60;
			int ss = (int) floor((st + 360.0 - hh * 15 - mm * 0.25) * 240) % 60;
			stprintf(server->getStream(), "%f\r\n", st);
			stprintf(server->getStream(), "%d:%d:%d LST\r\n", hh, mm, ss);
			return 0;
		}
		else if (strcmp(argv[0], "local") == 0)
		{
			t += (int) (remainder(server->getEqMount()->getLocation().lon, 360)
					* 240);
			ctime_r(&t, buf);
			stprintf(server->getStream(), "%s\r\n", buf);
			return 0;
		}

	}

	// Print of formatted string of current time
	ctime_r(&t, buf);
	stprintf(server->getStream(), "%s\r\n", buf);

	return 0;
}

static int eqmount_settime(EqMountServer *server, int argn, char *argv[])
{
	if (argn == 1)
	{
		//Use the first argument as UTC timestamp
		time_t t = strtol(argv[0], NULL, 10);
		server->getEqMount()->getClock().setTime(t);
	}
	else if (argn == 6)
	{
		int year = strtol(argv[0], NULL, 10);
		int month = strtol(argv[1], NULL, 10);
		int day = strtol(argv[2], NULL, 10);
		int hour = strtol(argv[3], NULL, 10);
		int min = strtol(argv[4], NULL, 10);
		int sec = strtol(argv[5], NULL, 10);
		struct tm ts;
		ts.tm_sec = sec;
		ts.tm_min = min;
		ts.tm_hour = hour;
		ts.tm_mday = day;
		ts.tm_mon = month - 1;
		ts.tm_year = year - 1900;
		ts.tm_isdst = 0;

		time_t t = mktime(&ts);
		if (t == -1)
		{
			// Parameter out of range
			return 2;
		}

		server->getEqMount()->getClock().setTime(t);
	}
	else
	{
		stprintf(server->getStream(),
				"Usage: settime <timestamp>, or, settime <year> <month> <day> <hour> <minute> <second> (UTC time should be used)\r\n");
		return 1;
	}

	return 0;
}

void EqMountServer::addCommand(const ServerCommand& cmd)
{
	int i = 0;
	while (i < MAX_COMMAND && commandlist[i].fptr != NULL)
		i++;
	if (i >= MAX_COMMAND - 1)
	{
		debug("Error: max command reached.\n");
		return;
	}

	commandlist[i] = cmd;
	commandlist[++i] = ServerCommand("", "", NULL);
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
		ServerCommand("speed", "Set slew and tracking speed", eqmount_speed), /// Set speed
		ServerCommand("align", "Star alignment", eqmount_align), /// Alignment
		ServerCommand("help", "Print this help menu", eqmount_help), /// Help menu
		ServerCommand("time", "Get and set system time", eqmount_time), /// System time
		ServerCommand("settime", "Set system time", eqmount_settime), /// System time
		ServerCommand("", "", NULL) };

