/*
 * EqMountServer.h
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#ifndef EQMOUNTSERVER_H_
#define EQMOUNTSERVER_H_

#include "MountServer.h"
#include "EquatorialMount.h"

class EqMountServer;

struct ServerCommand
{
	const char *cmd; /// Name of the command
	const char *desc; /// Description of the command
	int (*fptr)(EqMountServer *, int, char **); /// Function pointer to the command
	ServerCommand(const char *n = "", const char *d = "",
			int (*fp)(EqMountServer *, int, char **) = NULL) :
			cmd(n), desc(d), fptr(fp)
	{
	}
};

class EqMountServer: public MountServer
{
protected:

	EquatorialMount *eq_mount;
	FileHandle &stream;
	Thread thread;bool echo; /// Echo
	volatile bool commandRunning; // Whether a command is already running.

	void task_thread();

	void command_execute(ServerCommand &, int argn, char *argv[]);

public:
	EqMountServer(FileHandle &stream, bool echo = false);
	virtual ~EqMountServer();

	void bind(EquatorialMount &eq)
	{
		eq_mount = &eq;
	}

	EquatorialMount* getEqMount() const
	{
		return eq_mount;
	}

	FileHandle& getStream() const
	{
		return stream;
	}
};

#endif /* EQMOUNTSERVER_H_ */
