#include "mbed.h"
#include <stdio.h>
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "EquatorialMount.h"

Thread th;
DigitalOut led1(LED1);
//DigitalOut led1(PB_3_ALT0);
//DigitalOut led2(PC_12);
//DigitalOut led3(PE_3);
//DigitalOut led4(PB_7);

void blink()
{
    while (true) {
        led1 = !led1;
//		led2 = !led2;
//		led3 = !led3;
//		led4 = !led4;
        Thread::wait(600);
    }
}

Thread printer_th(osPriorityNormal, 2048, NULL, "Printer Thread");
/* Mail */
typedef struct {
    char msg[128];
} mail_t;

typedef Mail<mail_t, 256> MB_t;
MB_t mbox;

Timer tim;
void printer(MB_t *mbox)
{
    while (true) {
        mail_t *m = (mail_t *) mbox->get().value.p;
        printf("%s\r\n", m->msg);
        mbox->free(m);
    }
}

/**
 * Printf for debugging use. Takes about 20us for each call. Can be called from any context
 */
void xprintf(const char* format, ...)
{
    uint16_t len;
    va_list argptr;
    va_start(argptr, format);

    mail_t *m = mbox.alloc();
    len = sprintf(m->msg, "%6d>", tim.read_ms());
    len += vsprintf(&m->msg[len], format, argptr);
    mbox.put(m);

    va_end(argptr);
}

extern void test_stepper();
extern void testmath();

SDBlockDevice sdb(PA_7, PB_4, PA_5, PC_13);
FATFileSystem fs("fs");

char s[128];

Thread test;

int main()
{
    th.start(blink);

    tim.start();

    printer_th.start(callback(printer, &mbox));

    xprintf("System initialized");

    if (fs.mount(&sdb) == 0) {
        FILE *fp = fopen("/fs/Hello.txt", "r");
        if (fp == NULL) {
            xprintf("Could not open file for write\n");
        } else {
            fgets(s, 128, fp);
            xprintf("%s", s);
            fclose(fp);
        }

    } else {
        xprintf("FS failed to load SD card.");
    }

	testmath();
	
	

	test.start(test_stepper);


    while(1) { //
//		Thread::wait(1000);
//		time_t t = time(NULL);
//
//		xprintf("Time as seconds since January 1, 1970 = %d\n", t);
//
//        xprintf("Time as a basic string = %s", ctime(&t));
//
//        char buffer[32];
//        strftime(buffer, 32, "%I:%M %p\n", localtime(&t));
//        xprintf("Time as a custom formatted string = %s", buffer);

        EquatorialMount eq;
        double ra, dec;
        printf("Input RA and DEC: \r\n");
        scanf("%lf%lf", &ra, &dec);
        float tstart = tim.read();
        eq.correct_for_misalignment(&ra, &dec);
        tstart = tim.read()-tstart;
        printf("Corrected RA=%lf, DEC=%lf\r\nTime consumed: %f s", ra, dec, tstart);
    }
}


