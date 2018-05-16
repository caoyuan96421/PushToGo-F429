/*
 * USBSerial.cpp
 *
 *  Created on: 2018Äê4ÔÂ15ÈÕ
 *      Author: caoyuan9642
 */

#include "USBSerial.h"
#include "usbd_def.h"

InputQueue<char, USBSERIAL_QUEUE_SIZE> USBSerial::rxq;
OutputQueue<char, USBSERIAL_QUEUE_SIZE> USBSerial::txq;
USBD_HandleTypeDef USBSerial::hUSBDDevice;

static unsigned char rxbuf[USBSERIAL_QUEUE_SIZE];
static unsigned char txbuf[USBSERIAL_QUEUE_SIZE];

USBSerial USBSerial::instance;

USBD_CDC_LineCodingTypeDef linecoding =
{ 115200, /* baud rate*/
0x00, /* stop bits-1*/
0x00, /* parity - none*/
0x08 /* nb. of bits 8*/
};

int8_t USBSerial::CDC_Init(void)
{
	/*Start receiving*/
	USBD_CDC_SetRxBuffer(&hUSBDDevice, rxbuf);
	USBD_CDC_ReceivePacket(&hUSBDDevice);
	return 0;
}

int8_t USBSerial::CDC_DeInit(void)
{
	return 0;
}

int8_t USBSerial::CDC_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
	switch (cmd)
	{
	case CDC_SEND_ENCAPSULATED_COMMAND:
		/* Add your code here */
		break;

	case CDC_GET_ENCAPSULATED_RESPONSE:
		/* Add your code here */
		break;

	case CDC_SET_COMM_FEATURE:
		/* Add your code here */
		break;

	case CDC_GET_COMM_FEATURE:
		/* Add your code here */
		break;

	case CDC_CLEAR_COMM_FEATURE:
		/* Add your code here */
		break;

	case CDC_SET_LINE_CODING:
		linecoding.bitrate = (uint32_t) (pbuf[0] | (pbuf[1] << 8)
				|\
 (pbuf[2] << 16) | (pbuf[3] << 24));
		linecoding.format = pbuf[4];
		linecoding.paritytype = pbuf[5];
		linecoding.datatype = pbuf[6];

		/* Add your code here */
		break;

	case CDC_GET_LINE_CODING:
		pbuf[0] = (uint8_t) (linecoding.bitrate);
		pbuf[1] = (uint8_t) (linecoding.bitrate >> 8);
		pbuf[2] = (uint8_t) (linecoding.bitrate >> 16);
		pbuf[3] = (uint8_t) (linecoding.bitrate >> 24);
		pbuf[4] = linecoding.format;
		pbuf[5] = linecoding.paritytype;
		pbuf[6] = linecoding.datatype;

		/* Add your code here */
		break;

	case CDC_SET_CONTROL_LINE_STATE:
		/* Add your code here */
		break;

	case CDC_SEND_BREAK:
		/* Add your code here */
		break;

	default:
		break;
	}

	return (0);
}

void USBSerial::onotify(OutputQueue<char, USBSERIAL_QUEUE_SIZE>* txq)
{
	// Data available in output queue
	core_util_critical_section_enter();
	USBD_CDC_HandleTypeDef *hcdc =
			(USBD_CDC_HandleTypeDef*) hUSBDDevice.pClassData;
	unsigned int len = txq->count();

	if (hcdc->TxState == 0 && len > 0)
	{
		// No ongoing transmission
		unsigned int i;
		for (i = 0; i < len; i++)
		{
			txq->get((char*) &txbuf[i]);
		}
		USBD_CDC_SetTxBuffer(&hUSBDDevice, txbuf, i);
		USBD_CDC_TransmitPacket(&hUSBDDevice);
	}
	core_util_critical_section_exit();
}

void USBSerial::inotify(InputQueue<char, USBSERIAL_QUEUE_SIZE>* rxq)
{
	// Space available in queue
	core_util_critical_section_enter();
	USBD_CDC_HandleTypeDef *hcdc =
			(USBD_CDC_HandleTypeDef*) hUSBDDevice.pClassData;
	unsigned int space = rxq->capacity();
	if (hcdc->RxState == 0 && space >= CDC_DATA_FS_MAX_PACKET_SIZE)
	{
		// No ongoing transmission
		USBD_CDC_SetRxBuffer(&hUSBDDevice, rxbuf);
		USBD_CDC_ReceivePacket(&hUSBDDevice);
	}
	core_util_critical_section_exit();
}

int8_t USBSerial::CDC_Received(uint8_t* pbuf, uint32_t* Len)
{
	int len = *Len;
	if (len > rxq.capacity())
		len = rxq.capacity();
	for (int i = 0; i < len; i++)
	{
		rxq.put(pbuf[i]);
	}
	// Prepare for next receive
	inotify(&rxq);
	return 0;
}

int8_t USBSerial::CDC_Transmitted(uint8_t* pbuf, uint32_t* Len)
{
	// Check if anything to send
	onotify(&txq);
	return 0;
}

ssize_t USBSerial::read(void* buffer, size_t size)
{
	char *p = (char*) buffer;
	for (unsigned int i = 0; i < size; i++)
	{
		rxq.get(&p[i]);
	}
	return size;
}

ssize_t USBSerial::write(const void* buffer, size_t size)
{
	char *p = (char*) buffer;
	for (unsigned int i = 0; i < size; i++)
	{
		txq.put(p[i]);
	}
	return size;
}

off_t USBSerial::seek(off_t offset, int whence)
{
	return 0;
}

int USBSerial::close()
{
	return 0;
}
