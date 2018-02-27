/*
 * LCDStreamHandle.h
 *
 *  Created on: 2018Äê2ÔÂ25ÈÕ
 *      Author: caoyuan9642
 */

#ifndef LCDCONSOLE_H_
#define LCDSTREAMHANDLE_H_

#include "mbed.h"
#include "LCD_DISCO_F429ZI.h"

class LCDConsole: public FileLike
{
protected:

	static int* buffer; // The first 3 byets of each int contain its color, and last byte contain the content
	static int *head, *tail;
	static Mutex mutex;
	static LCD_DISCO_F429ZI lcd;
	static Thread thread;
	static bool inited;
	static int x0, y0, width, height;
	static int textwidth, textheight, buffersize;
	static Semaphore sem_update;

	uint32_t color;

	static void task_thread();
public:

	static void init(int x0, int y0, int width, int height);
	/** Redirects the stdout the stderr
	 *  @param tolcd If true, stdout and stderr will be redirected to the console.
	 *  If false, it will be redirected back to serial
	 */
	static void redirect(bool tolcd);

	// Can initialize multiple instances
	LCDConsole(const char *name, uint32_t color);
	virtual ~LCDConsole()
	{
	}

	/** Read the contents of a file into a buffer
	 *
	 *  Devices acting as FileHandles should follow POSIX semantics:
	 *
	 *  * if no data is available, and non-blocking set return -EAGAIN
	 *  * if no data is available, and blocking set, wait until some data is available
	 *  * If any data is available, call returns immediately
	 *
	 *  @param buffer   The buffer to read in to
	 *  @param size     The number of bytes to read
	 *  @return         The number of bytes read, 0 at end of file, negative error on failure
	 */
	virtual ssize_t read(void *buffer, size_t size);

	/** Write the contents of a buffer to a file
	 *
	 *  Devices acting as FileHandles should follow POSIX semantics:
	 *
	 * * if blocking, block until all data is written
	 * * if no data can be written, and non-blocking set, return -EAGAIN
	 * * if some data can be written, and non-blocking set, write partial
	 *
	 *  @param buffer   The buffer to write from
	 *  @param size     The number of bytes to write
	 *  @return         The number of bytes written, negative error on failure
	 */
	virtual ssize_t write(const void *buffer, size_t size);

	/** Move the file position to a given offset from from a given location
	 *
	 *  @param offset   The offset from whence to move to
	 *  @param whence   The start of where to seek
	 *      SEEK_SET to start from beginning of file,
	 *      SEEK_CUR to start from current position in file,
	 *      SEEK_END to start from end of file
	 *  @return         The new offset of the file, negative error code on failure
	 */
	virtual off_t seek(off_t offset, int whence = SEEK_SET);

	/** Close a file
	 *
	 *  @return         0 on success, negative error code on failure
	 */
	virtual int close();
	/** Check for poll event flags
	 * The input parameter can be used or ignored - the could always return all events,
	 * or could check just the events listed in events.
	 * Call is non-blocking - returns instantaneous state of events.
	 * Whenever an event occurs, the derived class should call the sigio() callback).
	 *
	 * @param events        bitmask of poll events we're interested in - POLLIN/POLLOUT etc.
	 *
	 * @returns             bitmask of poll events that have occurred.
	 */
	virtual short poll(short events) const
	{
		// Possible default for real files
		return POLLOUT;
	}
	/** Check if the file in an interactive terminal device
	 *
	 *  @return         True if the file is a terminal
	 *  @return         False if the file is not a terminal
	 *  @return         Negative error code on failure
	 */
	virtual int isatty()
	{
		return true;
	}
};

#endif /* LCDCONSOLE_H_ */
