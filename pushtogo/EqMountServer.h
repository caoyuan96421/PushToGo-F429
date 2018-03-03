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

class EqMountServer: public MountServer
{
protected:
	EquatorialMount *eq_mount;
	Stream &stream;
	Thread thread;
	bool echo;

	static void task_thread(EqMountServer *server);

public:
	EqMountServer(Stream &stream, bool echo = false);
	virtual ~EqMountServer();

	void bind(EquatorialMount &eq)
	{
		eq_mount = &eq;
	}


};

#endif /* EQMOUNTSERVER_H_ */
