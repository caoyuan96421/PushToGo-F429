/*
 * LCDStreamHandle.cpp
 *
 *  Created on: 2018Äê2ÔÂ25ÈÕ
 *      Author: caoyuan9642
 */

#include "LCDConsole.h"
#include <math.h>
#include <ctype.h>
#include "MCULoadMeasurement.h"

bool LCDConsole::inited = false;
Mutex LCDConsole::mutex;
LCD_DISCO_F429ZI LCDConsole::lcd;
int LCDConsole::x0 = 0, LCDConsole::y0 = 0, LCDConsole::width = lcd.GetXSize(),
		LCDConsole::height = lcd.GetYSize();
int LCDConsole::textheight = 0, LCDConsole::textwidth = 0,
		LCDConsole::buffersize = 0;
int *LCDConsole::buffer, *LCDConsole::head, *LCDConsole::tail;
Semaphore LCDConsole::sem_update(0, 1);
Thread LCDConsole::thread(osPriorityLow, OS_STACK_SIZE, NULL, "LCD Console");

LCDConsole lcd_handle_out("lcd_stdout", LCD_COLOR_WHITE);
LCDConsole lcd_handle_err("lcd_stderr", LCD_COLOR_YELLOW);

#define BG_COLOR LCD_COLOR_BLACK

void idle_hook()
{
	core_util_critical_section_enter();
	MCULoadMeasurement::getInstance().setMCUActive(false);
	sleep_manager_lock_deep_sleep();
	sleep();
	sleep_manager_unlock_deep_sleep();
	MCULoadMeasurement::getInstance().setMCUActive(true);
	core_util_critical_section_exit();
}

void LCDConsole::init(int x0, int y0, int width, int height)
{
	LCDConsole::x0 = x0;
	LCDConsole::y0 = y0;
	LCDConsole::width = width;
	LCDConsole::height = height;

	// Clear area with background color
	lcd.SetTextColor(BG_COLOR);
	lcd.FillRect(x0, y0, width, height);
	BSP_LCD_SetFont(&Font12);

	// Calculate width and height of the text area
	textwidth = width / BSP_LCD_GetFont()->Width;
	textheight = height / BSP_LCD_GetFont()->Height;
	buffersize = textwidth * textheight;

	// Init buffer
	buffer = new int[buffersize](); // Will be init to zero
	head = tail = buffer;

	// Start task thread
	thread.start(task_thread);

	// Register idle hook
	Thread::attach_idle_hook(idle_hook);
	MCULoadMeasurement::getInstance().reset();
}

void LCDConsole::task_thread()
{
	int *buffer0 = new int[buffersize](); // Local buffer
	char sbuf[64];
	// Main loop
	while (true)
	{
		// Wait for update signal.
		int s = sem_update.wait(1000);
		if (s == 0)
		{
			// Timeout, update CPU usage
			lcd.SetBackColor(LCD_COLOR_BLUE);
			lcd.SetTextColor(LCD_COLOR_WHITE);
			int len = sprintf(sbuf, "Load: %4.1f%% ",
					MCULoadMeasurement::getInstance().getCPUUsage() * 100);
			lcd.DisplayStringAt(0, lcd.GetYSize() - BSP_LCD_GetFont()->Height,
					(unsigned char*) sbuf, LEFT_MODE);
			MCULoadMeasurement::getInstance().reset();

			time_t t = time(NULL);
			struct tm ts;
			gmtime_r(&t, &ts);
			strftime(sbuf, sizeof(sbuf), "%T, %x", &ts);
			strcat(sbuf, " UTC"); // append

			lcd.SetBackColor(0xFF00AF7F);
			lcd.DisplayStringAt(BSP_LCD_GetFont()->Width * len,
					lcd.GetYSize() - BSP_LCD_GetFont()->Height,
					(unsigned char*) sbuf, LEFT_MODE);

			continue;
		}

		mutex.lock(); // Lock the buffer. If any thread is still printing stuff to the buffer, this will wait for it to finish
		memcpy(buffer0, buffer, buffersize * sizeof(*buffer)); // Copy buffer to a private one to work in
		mutex.unlock(); // Unlock the buffer. If any thread is waiting for the mutex, it can now run (and potentially generating another update signal).

		int *head0 = buffer0 + (head - buffer);
		// clear area;
//		lcd.SetTextColor(BG_COLOR);
//		lcd.FillRect(x0, y0, width, height);

		int line = 0, col = 0;
		int x = x0, y = y0;
		lcd.SetBackColor(BG_COLOR);
		for (int *p = head0, i = 0; i < buffersize; i++)
		{
			// Set color from the buffer
			lcd.SetTextColor((uint32_t(0xFF000000 | (*p >> 8))));

			// Content to draw
			unsigned char c = (*p) & 0xFF;

			// Display the char if displayable, otherwise put white space
			if (isprint(c))
				lcd.DisplayChar(x, y, c);
			else
				lcd.DisplayChar(x, y, ' ');

			x += BSP_LCD_GetFont()->Width;
			col++;
			if (col == textwidth)
			{
				// Next line
				line++;
				y += BSP_LCD_GetFont()->Height;
				col = 0;
				x = 0;
			}
			p++;
			if (p - buffer0 == buffersize)
				p = buffer0; // wrap around the buffer
		}
	}
}

LCDConsole::LCDConsole(const char * name, uint32_t color) :
		FileLike(name)
{
	this->color = color;
}

ssize_t LCDConsole::read(void* buffer, size_t size)
{
// Not supported
	return -1;
}

ssize_t LCDConsole::write(const void* str, size_t size)
{
	mutex.lock();
	char *pb = (char*) str;
	for (char *p = pb; p < pb + size; p++)
	{
		char c = (int) (*p);
		bool scroll = false;
		if (isprint(c))
		{
			*(tail++) = (color << 8) | c; // Put the current char and color into the buffer
			if (tail >= buffer + buffersize)
				tail -= buffersize;
			if (tail == head)
				scroll = true;
		}
		else if (c == '\b')
		{
			// Backspace, tail pointer go back by one
			if (tail != head) // If the buffer is empty, do nothing
			{
				tail--;
				if (tail < buffer)
					tail += buffersize;
			}

		}
		else if (c == '\r')
		{
			// Roll back to start of line
			int currpos = (tail - head + buffersize) % buffersize; // current position in buffer
			int linestart = currpos - currpos % textwidth; // Position of the start of the line
			tail = head + linestart;
			if (tail >= buffer + buffersize)
				tail -= buffersize;
		}
		else if (c == '\n')
		{
			// Newline
			int currpos = (tail - head + buffersize) % buffersize; // current position in buffer
			int nextlinestart = currpos - currpos % textwidth + textwidth; // Position of a new line start
			tail = head + nextlinestart;
			if (tail >= buffer + buffersize)
				tail -= buffersize;
			if (nextlinestart >= buffersize) // Scroll if the tail will overrun the head
				scroll = true;
		}

		// wrap

		if (scroll)
		{
			// Scroll a line
			head += textwidth; // Increase head by one line
			if (head >= buffer + buffersize)
			{
				head -= buffersize; // wrap
			}
			for (int *p = tail; p != head;)
			{
				*(p++) = 0; // Set everything between tail and head to null
				if (p >= buffer + buffersize)
				{
					p -= buffersize;
				}
			}
		}
	}
	sem_update.release(); // Signal the task to update the graphics

	mutex.unlock();
	return size;
}

off_t LCDConsole::seek(off_t offset, int whence)
{
// Not supported
	return -1;
}

int LCDConsole::close()
{
	return 0;
}

void LCDConsole::redirect(bool tolcd)
{
	if (tolcd)
	{
		freopen("/lcd_stdout", "w", stdout);
		freopen("/lcd_stderr", "w", stderr);
	}
	else
	{
		freopen("/stdout", "w", stdout);
		freopen("/stderr", "w", stderr);
	}
}

// Override the default fatal error handler
extern "C" void error(const char* format, ...)
{
	static unsigned char buffer[257];
	core_util_critical_section_enter();
#ifndef NDEBUG

	LCD_DISCO_F429ZI lcd; // Use local copy of the lcd, so that this function can be used at any stage of the initialization.
	va_list arg;
	va_start(arg, format);
	int size = vsnprintf((char*) buffer, 256, format, arg);
	if (size > 256)
	{
		// Properly terminate the string
		size = 256;
		buffer[256] = '\0';
	}
	va_end(arg);

	BSP_LCD_SetFont(&Font20);

	lcd.Clear(LCD_COLOR_WHITE);
	lcd.SetBackColor(LCD_COLOR_WHITE);
	lcd.SetTextColor(LCD_COLOR_RED);

	int i = 0, line = 0;
	while (i < size)
	{
		unsigned char c = buffer[i + 16];
		buffer[i + 16] = '\0';
		lcd.DisplayStringAtLine(line++, buffer + i);
		buffer[i + 16] = c;
		i += 16;
	}
#endif

	core_util_critical_section_exit();
	exit(1);
}

