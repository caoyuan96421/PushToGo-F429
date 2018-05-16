/*
 * USBSerial.h
 *
 *  Created on: 2018Äê4ÔÂ15ÈÕ
 *      Author: caoyuan9642
 */

#ifndef USB_USBSERIAL_H_

#define USB_USBSERIAL_H_

#include "FileHandle.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "IOQueue.h"

#define USBSERIAL_QUEUE_SIZE (2 * CDC_DATA_FS_MAX_PACKET_SIZE)

class USBSerial: public mbed::FileHandle
{
private:
	static USBD_HandleTypeDef hUSBDDevice;
	static USBSerial instance;
	static int8_t CDC_Init(void);
	static int8_t CDC_DeInit(void);
	static int8_t CDC_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length);
	static int8_t CDC_Received(uint8_t* pbuf, uint32_t *Len);
	static int8_t CDC_Transmitted(uint8_t* pbuf, uint32_t *Len);

	static void onotify(OutputQueue<char, USBSERIAL_QUEUE_SIZE> *);
	static void inotify(InputQueue<char, USBSERIAL_QUEUE_SIZE> *);

	static InputQueue<char, USBSERIAL_QUEUE_SIZE> rxq;
	static OutputQueue<char, USBSERIAL_QUEUE_SIZE> txq;

	USBSerial()
	{
		USBD_Init(&hUSBDDevice, &USB_Desc, 0);
		USBD_RegisterClass(&hUSBDDevice, USBD_CDC_CLASS);

		static USBD_CDC_ItfTypeDef cdc_callbacks =
		{ CDC_Init, CDC_DeInit, CDC_Control, CDC_Received, CDC_Transmitted };

		USBD_CDC_RegisterInterface(&hUSBDDevice, &cdc_callbacks);
		USBD_Start(&hUSBDDevice);
		rxq.notify(inotify);
		txq.notify(onotify);
	}
	virtual ~USBSerial()
	{
		USBD_Stop(&hUSBDDevice);
		USBD_DeInit(&hUSBDDevice);
	}
public:

	static USBSerial &getInstance()
	{
		return instance;
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
	ssize_t read(void *buffer, size_t size);

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
	ssize_t write(const void *buffer, size_t size);

	/** Move the file position to a given offset from from a given location
	 *
	 *  @param offset   The offset from whence to move to
	 *  @param whence   The start of where to seek
	 *      SEEK_SET to start from beginning of file,
	 *      SEEK_CUR to start from current position in file,
	 *      SEEK_END to start from end of file
	 *  @return         The new offset of the file, negative error code on failure
	 */
	off_t seek(off_t offset, int whence = SEEK_SET);

	/** Close a file
	 *
	 *  @return         0 on success, negative error code on failure
	 */
	int close();
	/** Check if the file in an interactive terminal device
	 *
	 *  @return         True if the file is a terminal
	 *  @return         False if the file is not a terminal
	 *  @return         Negative error code on failure
	 */
	int isatty()
	{
		return true;
	}

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
	short poll(short events) const
	{
		// Possible default for real files
		return POLLIN | POLLOUT;
	}
};

#endif /* USB_USBSERIAL_H_ */
