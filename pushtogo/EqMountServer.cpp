/*
 * EqMountServer.cpp
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#include "EqMountServer.h"
#include <ctype.h>

#define EMS_DEBUG 1

EqMountServer::EqMountServer(Stream &stream, bool echo) :
		eq_mount(NULL), stream(stream), thread(osPriorityBelowNormal,
		OS_STACK_SIZE, NULL, "MountServer"), echo(echo)
{
	thread.start(callback(task_thread, this));
}

EqMountServer::~EqMountServer()
{
	thread.terminate();
}

void EqMountServer::task_thread(EqMountServer *server)
{
	char buffer[256]; // text buffer
	while (true)
	{
		bool eof = false;
		if (server->echo)
		{
			int x = 0;
			int i = 0;
			while (!eof && i < 255)
			{
				if(server->stream.readable()) // Check if there is anything to read, so that we don't block
					x = server->stream.getc();
				else{
					// Wait for next tick
					Thread::wait(1);
					continue;
				}
				if (x == EOF)
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
						server->stream.puts("\b \b"); // blank the current character properly
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
				server->stream.putc(x);
				buffer[i++] = (char) x;
			}
			if (eof && i == 0)
				break;
			server->stream.putc('\r'); // Put new line character after command
			server->stream.putc('\n'); // Put new line character after command
			if (i == 0) // Empty command
				continue;
			buffer[i] = 0; // insert null character
			debug_if(EMS_DEBUG, "command: |%s|\n", buffer);
		}
		else
		{
			if (server->stream.gets(buffer, 256) == NULL) // Get new command
				break;
			if (buffer[0] == 0) // Empty command
				continue;
		}

		if (server->eq_mount == NULL)
		{
			server->stream.puts("Error: EqMount not binded.\r\n");
			continue;
		}


	}
	// If we reach here, it must be end of file
}
