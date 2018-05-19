/*
 * EqMountServer.h
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#ifndef EQMOUNTSERVER_H_
#define EQMOUNTSERVER_H_

class EqMountServer;

#include "MountServer.h"
#include "EquatorialMount.h"

struct ServerCommand
{
	const char *cmd; /// Name of the command
	const char *desc; /// Description of the command
	int (*fptr)(EqMountServer *, const char *, int, char **); /// Function pointer to the command
	ServerCommand(const char *n = "", const char *d = "",
			int (*fp)(EqMountServer *, const char *, int, char **) = NULL) :
			cmd(n), desc(d), fptr(fp)
	{
	}
};

#define MAX_COMMAND 128

#define ERR_WRONG_NUM_PARAM 1
#define ERR_PARAM_OUT_OF_RANGE 2

class EqMountServer: public MountServer
{
protected:

	EquatorialMount *eq_mount;
	FileHandle &stream;
	Thread thread;bool echo; /// Echo

	void task_thread();

	void command_execute(ServerCommand &, int argn, char *argv[], char *buffer);

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

	static void addCommand(const ServerCommand &cmd);
};

/**
 * Print to stream
 */
void stprintf(FileHandle &f, const char *fmt, ...);

#endif /* EQMOUNTSERVER_H_ */
