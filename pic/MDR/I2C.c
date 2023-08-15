#define __16f1847
#include <pic16f1847.h>
#include <stdint.h>
#include "I2C.h"

/* set clock high (default state) */
#define release_SCL() LATB |= SCL
/* set clock low */
#define hold_SCL() LATB &= ~SCL
/* set data high (default state) */
#define release_SDA() LATB |= SDA
/* set data low */
#define hold_SDA() LATB &= ~SDA
/* a small delay for better clock */
#define delay() __asm\
	nop\
	nop\
	nop\
	nop\
__endasm

/* read SDA
 * set SDA as input in the begining
 * set SDA as output in the end
 */
uint8_t read_ack(void)
{
	uint8_t ack;
	hold_SCL(); /* set clock to low for SDA to change */
	TRISB |= SDA; /* set as input */
	delay(); /* wait for SDA to change */
	release_SCL(); /* SDA should be valid when clock is high */
	delay();
	/* we don't verify the SCL state as only we can drive it */
	if (PORTB&SDA) { /* read bit */
		ack = 1;
	} else {
		ack = 0;
	}
	hold_SCL(); /* put clock to low, as safe state */
	release_SDA(); /* set data to default state */
	TRISB &= ~SDA; /* set back to output */
	/* leave the output value as it is */
	return ack;
}

/* sent start condition
 * falling SDA while SCL is high
 */
void send_start(void)
{
	release_SCL(); /* endure SCL is high */
	delay();
	release_SDA(); /* ensure SDA is high to be able to start */
	delay();
	hold_SDA(); /* send start signal */
}

/* send stop condition
 * rising SDA while SCL is high
 */
void send_stop(void)
{
	hold_SDA(); /* be sure SDA is low for rising edge */
	delay();
	release_SCL(); /* be sure SCL is high */
	delay();
	release_SDA(); /* set SDA high while SCL is high */
}

/* send byte and get ack (0=ack) */
uint8_t send_byte(uint8_t byte)
{
	uint8_t bit, ack;
	/* send every bit, MSb first */
	for (bit = 0; bit < 8; bit++) {
		hold_SCL(); /* value can change when SCL is low */
		delay();
		if (byte&0x80) {
			release_SDA(); /* set high for 1 */
		} else {
			hold_SDA(); /* set low for 0 */
		}
		delay();
		release_SCL(); /* set clock high to indicate valid value */
		byte <<= 1;
	}
	/* read ack */
	delay();
	hold_SCL(); /* set clock to low for SDA to change */
	TRISB |= SDA; /* set as input */
	delay(); /* wait for SDA to change */
	delay();
	delay();
	release_SCL(); /* SDA should be valid when clock is high */
	delay();
	/* we don't verify the SCL state as only we can drive it */
	if (PORTB&SDA) { /* read bit */
		ack = 1;
	} else {
		ack = 0;
	}
	hold_SCL(); /* put clock to low, as safe state */
	delay();
	release_SDA(); /* set output to high (default) */
	TRISB &= ~SDA; /* set back to output */
	return ack;
}

/* read one byte and send an ack
 * send an ack if you want to read more bytes
 * send a nack if this is the last byte read
 */
uint8_t read_byte(uint8_t ack)
{
	uint8_t byte = 0;
	uint8_t bit;
	hold_SCL(); /* set clock to low for SDA to change */
	TRISB |= SDA; /* set as input */
	for (bit=0; bit<8; bit++) {
		delay(); /* wait for SDA to change */
		release_SCL(); /* SDA should be valid when clock is high */
		byte <<= 1; /* make place to save the next bit */
		if (PORTB&SDA) { /* read bit */
			byte += 1;
		} else {
			byte += 0;
		}
		hold_SCL(); /* set clock to low for SDA to change */
	}
	if (ack) {
		hold_SDA(); /* set low for ack */
	} else {
		release_SDA(); /* set high for nack */
	}
	TRISB &= ~SDA; /* set back to output */
	delay();
	release_SCL(); /* set clock high to indicate valid value */
	delay();
	hold_SCL(); /* set clock to low for SDA to change */
	return byte;
}
