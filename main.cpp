#include "mbed.h"
#include <LCDConsole.h>
#include <stdio.h>
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "EquatorialMount.h"
#include "MCULoadMeasurement.h"

Thread th;
DigitalOut led1(LED1);
//DigitalOut led1(PB_3_ALT0);
//DigitalOut led2(PC_12);
//DigitalOut led3(PE_3);
//DigitalOut led4(PB_7);

void blink()
{
	while (true)
	{
		led1 = !led1;
//		led2 = !led2;
//		led3 = !led3;
//		led4 = !led4;
		Thread::wait(600);
	}
}

//Thread printer_th(osPriorityNormal, 2048, NULL, "Printer Thread");
///* Mail */
//typedef struct
//{
//	char msg[128];
//} mail_t;
//
//typedef Mail<mail_t, 256> MB_t;
//MB_t mbox;
//Serial pc(USBTX, USBRX, 115200);
//
//Timer tim;
//void printer(MB_t *mbox)
//{
//	while (true)
//	{
//		mail_t *m = (mail_t *) mbox->get().value.p;
//		pc.printf("%s\r\n", m->msg);
//		mbox->free(m);
//	}
//}

/**
 * Printf for debugging use. Takes about 20us for each call. Can be called from any context
 */
//void xprintf(const char* format, ...)
//{
//	uint16_t len;
//	va_list argptr;
//	va_start(argptr, format);
//
//	mail_t *m = mbox.alloc();
//	len = sprintf(m->msg, "%6d>", tim.read_ms());
//	len += vsprintf(&m->msg[len], format, argptr);
//	mbox.put(m);
//
//	va_end(argptr);
//}
extern void test_stepper();
extern void testmath();
extern void test_em();
extern void test_deapply();
extern void test_server();

//SDBlockDevice sdb(PA_7, PB_4, PA_5, PC_13);
//FATFileSystem fs("fs");

char s[128];

Thread test(osPriorityNormal, 16384, NULL, "test");

// Instrumentation
MCULoadMeasurement mcuload;
void idle_hook()
{
	core_util_critical_section_enter();
	mcuload.setMCUActive(false);
	sleep_manager_lock_deep_sleep();
	sleep();
	sleep_manager_unlock_deep_sleep();
	mcuload.setMCUActive(true);
	core_util_critical_section_exit();
}

int main()
{
	/*Enable LCD redirecting*/
	LCDConsole::init(0, 0, 240, 280);
	LCDConsole::redirect(true);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

// Register idle_hook
	Thread::attach_idle_hook(idle_hook);

	printf("System initialized.\n");

	th.start(blink);
//	tim.start();
//	printer_th.start(callback(printer, &mbox));
//	xprintf("System initialized");
//
//	if (fs.mount(&sdb) == 0)
//	{
//		FILE *fp = fopen("/fs/Hello.txt", "r");
//		if (fp == NULL)
//		{
//			xprintf("Could not open file for write\n");
//		}
//		else
//		{
//			fgets(s, 128, fp);
//			xprintf("%s", s);
//			fclose(fp);
//		}
//
//	}
//	else
//	{
//		xprintf("FS failed to load SD card.");
//	}

//	testmath();

//	test_deapply();
	test.start(test_em);

	LCD_DISCO_F429ZI &lcd = LCDConsole::getLCD();
	char buf[64];

	while (1)
	{
//		uint64_t timeNow = tim.read_high_resolution_us();
//		printf("Hello world! ");
//		fprintf(stderr, " asdjfk jaskldfj klasdjklf jklasdf %f %lld us\n", tim.read(),
//				tim.read_high_resolution_us() - timeNow);
//		printf("\r%d", i++);
		mcuload.reset();
		Thread::wait(1000);
		float load = mcuload.getCPUUsage();

		/*Display CPU usage*/
		lcd.SetBackColor(LCD_COLOR_BLUE);
		lcd.SetTextColor(LCD_COLOR_WHITE);
		sprintf(buf, "CPU Usage: %4.1f%%", load * 100);
		lcd.DisplayStringAt(0, lcd.GetYSize() - BSP_LCD_GetFont()->Height,
				(unsigned char*) buf, LEFT_MODE);
	}
}

