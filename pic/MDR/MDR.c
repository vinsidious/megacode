/* micro-controller firmware fot the Linear MDR/MDR2/MDR-U receiver
   Copyright (C) 2014 Kévin Redon <kingkevin@cuvoodoo.info>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */
/* libraries */
#define __16f1847
#include <pic16f1847.h>
#include <stdint.h>
#include "I2C.h"

/* the peripherals connected to the pins */
#define RELAY1 _RA2 /* pin 1 */
#define RELAY2 _RA3 /* pin 2 only for MDR2 */
/* pin 3 is connected to ground */
/* pin 4 is used as master clear */
/* pin 5 is Vss (ground) */
#define SWITCH1 _RB0 /* pin 6 (on ground when pressed) */
#define LED _RB1 /* pin 7 (used as sink) */
#define SWITCH2 _RB2 /* pin 8 (on ground when pressed) */
/* pin 9 is to identify board. Vdd for MDR, ground for MDR-U */
/* pin 10 is not connected */
/* pin 11 in not connected */
/* pin 12, external 24LC256 EEPROM */
/* pin 13, external 24LC256 EEPROM */
/* pin 14 is Vdd (5V) */
/* pin 15 is clonnected to external 4MHz ceramic resonator */
/* pin 16 is clonnected to external 4MHz ceramic resonator */
#define RADIO _RA0 /* radio signal receiver, filtered by LM358N */
/* pin 18 is to identify board. Vdd for MDR, ground for MDR-U */

/* simple functions */
#define led_off() LATB |= LED
#define led_on() LATB &= ~(LED)
#define sleep() __asm sleep __endasm

/* variables */
static uint8_t switches; /* save the last switch state */
#define START 208 /* starting timer 0 value to wait up to 12ms in start_timer*/
static uint8_t code[3] = {0xf1, 0x11, 0x11}; /* the received code (24 bits) */
static uint8_t new = 0; /* has a new code been detected (clear using button) */
static uint8_t hold = 0; /* how long has the button been held, in 250ms steps */

/* configuration bits */
uint16_t __at(_CONFIG1) __CONFIG1 = _FCMEN_ON & /* enable fail-safe clock monitor */
                                    _IESO_ON & /* enable internal/external switchover (since fail-safe is enabled) */
                                    _CLKOUTEN_ON & /* use CLKOUT for extrenal resonator (but it is ignored) */
                                    _BOREN_NSLEEP & /* only use brown out during normal operation (not sleep) */
                                    _CPD_OFF & /* no data memory code protection */
                                    _CP_OFF & /* no program memory code protection */
                                    _MCLRE_ON& /* use master clear */
                                    _PWRTE_ON & /* use power-up timer */
                                    _WDTE_OFF & /* disable watchdog */
                                    _FOSC_XT; /* use external resonator */
uint16_t __at(_CONFIG2) __CONFIG2 = _LVP_ON & /* enable low voltage programming */
                                    /* DEBUG is not present in library, but I don't use it */
                                    _BORV_LO & /* low borwn out voltage */
                                    _STVREN_ON & /* enable reset on stack overflow */
                                    _PLLEN_OFF & /* don't use 4X PLL, for longer timer */
                                    _WRT_OFF; /* no flash write protect */

/* initialize micro-conroller */
void init (void) {
	/* configure IO */
	ANSELA = 0; /* all pins are digital */
	ANSELB = 0; /* all pins are digital */
	TRISA |= RADIO; /* radio signal is an input */
	TRISA &= ~(RELAY1|RELAY2); /* relays are outputs */
	LATA &= ~(RELAY1|RELAY2); /* switch relays off */
	TRISB |= SWITCH1|SWITCH2; /* switches are inputs (with external pull-ups) */
	TRISB &= ~(LED); /* LED is an output (used a sink) */
	LATB |= LED; /* switch LED off */

	/* configure switches input */
	IOCIE = 1; /* enable interrupt on individual GPIO (typo in the lib) */
	IOCIF = 0; /* clear GPIO interrupt */
	IOCBP |= (SWITCH1|SWITCH2); /* enable interrupt when switch is released */
	IOCBN |= (SWITCH1|SWITCH2); /* enable interrupt when switch is pressed */
	switches = PORTB; /* save current switch state */

	/* use timer 0 to measure bitframes timing */
	TMR0CS = 1; /* disable timer 0 (T0CKI/RA4 is connected to ground) */
	TMR0SE = 0; /* increment timer0 on low to high edge ((T0CKI/RA4 is connected to ground) */
	PSA = 0; /* use presacler for timer 0 */
	PS0 = 1; /* use prescaler of 256 */
	PS1 = 1; /* use prescaler of 256 */
	PS2 = 1; /* use prescaler of 256 */
	TMR0IE = 1; /* enable timer 0 interrupt */
	TMR0IF = 0; /* clear timer 0 interrupt */

	/* use timer 2 to measure pulse durations (max 16.384ms) */
	TMR2ON = 0; /* stop timer 2 */
	T2CKPS0 = 1; /* use prescaler of 64 */
	T2CKPS1 = 1; /* use prescaler of 64 */
	T2OUTPS0 = 0; /* use postscale of 1 */
	T2OUTPS1 = 0; /* use postscale of 1 */
	T2OUTPS2 = 0; /* use postscale of 1 */
	T2OUTPS3 = 0; /* use postscale of 1 */
	PR2 = 0xff; /* set interrupt on overflow */
	TMR2IE = 1; /* enable timer 2 interrupt */
	TMR2IF = 0; /* clear timer 2 interrupt */

	/* use timer 4 to measure button hold (250ms per interrupt) */
	TMR4ON = 0; /* stop timer 4 */
	T4CKPS0 = 1; /* use prescaler of 64 */
	T4CKPS1 = 1; /* use prescaler of 64 */
	T4OUTPS0 = 1; /* use postscale of 16 */
	T4OUTPS1 = 1; /* use postscale of 16 */
	T4OUTPS2 = 1; /* use postscale of 16 */
	T4OUTPS3 = 1; /* use postscale of 16 */
	PR4 = 244; /* set interrupt on overflow on this value to get 250ms */
	TMR4IE = 1; /* enable timer 4 interrupt */
	TMR4IF = 0; /* clear timer 4 interrupt */

	/* the MDR originally uses a PIC16C54A (PDIP for MDR/MDR2, SOIC for MDR-U)
	 * this does does not provide hardware I2C function
	 * they implement I2C in software (it's even a bit buggy)
	 * it even does not used open collectors (with pull-ups), but drives the signals
	 * this PIC16F1847 offers hardware I2C, but not on those pins
	 * so we also have to use software bit banging I2C
	 */
	TRISB &= ~(SCL|SDA); /* set as output (it must be driven because there is no pull-up */
	LATB |= SCL|SDA; /* set as high (default pull-up state) */ 

	PEIE = 1; /* enable peripheral interrupt (for timer 2) */
	GIE = 1; /* golablly enable interrupts */
}

/* start timer to measure bitframe
 * returns the time (in 0.256ms steps) since the last start
 * stops after 12ms (then it returns 0)
 */
uint8_t start_bit_timer(void)
{
	/* uses timer 0
	 * speed is Fosc/4 with a prescaler of 256
	 * each tick should be 1.0/((4000000/4)/256) = 0.0002560163850486431 seconds
	 * since there should be one pulse every 6ms, only wait up to 12ms
	 */
	uint8_t time = TMR0-START; /* remember the time passed */
	TMR0 = START; /* reset timer */
	TMR0CS = 0; /* enable timer 0 */
	return time;
}

/* start timer to measure pulse length
 * read the time from TMR2
 * step is 0.064ms
 * stops at 0xff (16.384ms)
 */
void start_pulse_timer()
{
	TMR2 = 0; /* reset timer counter */
	TMR2ON = 1; /* start timer 2 */
}

/* write code (global variable) in EEPROM at specifoed address */
void write_code(uint16_t address)
{
	/* use I2C to select address */
	send_start();
	if (send_byte(0xa0)) { /* write eeprom at 0xA0/0x50 */
		send_stop();
		return;
	}
	if (send_byte((uint8_t)(address>>8))) { /* go to address */
		send_stop();
		return;
	}
	if (send_byte((uint8_t)(address&0xff))) { /* go to address */
		send_stop();
		return;
	}
	/* write bytes */
	if (send_byte(code[0])) {
		send_stop();
		return;
	}
	if (send_byte(code[1])) {
		send_stop();
		return;
	}
	if (send_byte(code[2])) {
		send_stop();
		return;
	}
	/* finish transaction */
	send_stop();
}

/* look in the memory for the code (global variable)
 * if not present, write it a the next free memory space
 * return 0 if the code is already in EEPROM
 * return 1 if the code is new and saved
 * return 2 if error occured
 * return 3 if no space in memory
 */
uint8_t save_code(void)
{
	uint16_t address;
	uint8_t stored[3];
	/* start at begining of the memory */
	send_start();
	if (send_byte(0xa0)) { /* write address to eeprom at 0xA0/0x50 */
		send_stop();
		return 2;
	}
	if (send_byte(0x00)) { /* go to address 0x0000 */
		send_stop();
		return 2;
	}
	if (send_byte(0x00)) { /* go to address 0x0000 */
		send_stop();
		return 2;
	}
	/* start reading */
	send_start();
	if (send_byte(0xa1)) { /* read eeprom at 0xA0/0x50 */
		send_stop();
		return 2;
	}
	/* go through memory */
	for (address=0; address<0x7FFF-2; address+=3) {
		/* read stored code */
		stored[0] = read_byte(1);
		stored[1] = read_byte(1);
		stored[2] = read_byte(1);
		if ((stored[0]&0x80)==0) { /* code always have the MSb to 1 */
			read_byte(0); /* send a NACK to stop reading */
			send_stop(); /* finish transaction */
			write_code(address); /* write code at this space */
			return 1;
		} else if (stored[0]==code[0] && stored[1]==code[1] && stored[2]==code[2]) { /* code already stored */
			read_byte(0); /* send a NACK to stop reading */
			send_stop(); /* finish transaction */
			return 0;
		}
	}
	return 3;
}

void clear_memory(void)
{
	uint16_t address;
	uint16_t wait;
	/* go through memory */
	for (address=0; address<0x7FFF; address++) {
		if ((address%0x40)==0) { /* select page */
			send_start(); /* start transaction */
			if (send_byte(0xa0)) { /* write address to eeprom at 0xA0/0x50 */
				send_stop();
				return;
			}
			if (send_byte(address>>8)) { /* go to address */
				send_stop();
				return;
			}
			if (send_byte(address&0xff)) { /* go to address */
				send_stop();
				return;
			}
		}
		send_byte(0x00); /* clear byte */
		if ((address%0x40)==0x3f) { /* end of page */
			send_stop(); /* finish transaction */
			for (wait=0; wait<1024; wait++); /* wait for eeprom to be writen */
		}
	}
}

/* read all codes from memory */
void dump_codes(void)
{
	uint16_t address;
	uint8_t stored[3];
	/* start at begining of the memory */
	send_start();
	if (send_byte(0xa0)) { /* write address to eeprom at 0xA0/0x50 */
		send_stop();
		return;
	}
	if (send_byte(0x00)) { /* go to address 0x0000 */
		send_stop();
		return;
	}
	if (send_byte(0x00)) { /* go to address 0x0000 */
		send_stop();
		return;
	}
	/* start reading */
	send_start();
	if (send_byte(0xa1)) { /* read eeprom at 0xA0/0x50 */
		send_stop();
		return;
	}
	/* go through memory */
	for (address=0; address<0x7FFF-2; address+=3) {
		/* read stored code */
		stored[0] = read_byte(1);
		stored[1] = read_byte(1);
		stored[2] = read_byte(1);
		if ((stored[0]&0x80)==0) { /* code always have the MSb to 1 */
			read_byte(0); /* send nack */
			send_stop(); /* end transaction */
			return; /* all codes have been read */
		}
	}
}

/* funcion called on interrupts */
/* interrupt 0 is only one on PIC16 */
static void interrupt(void) __interrupt 0
{
	if (IOCIF) { /* GPIO interrupt (typo in library?) */
		if (IOCBF&(SWITCH1|SWITCH2)) { /* switch activity */
			switches ^= PORTB; /* figure out which switch changed */
			if (switches&SWITCH1) { /* switch 1 changed */
				if (PORTB&SWITCH1) { /* switch 1 released */
					led_off(); /* reset LED */
					new = 0; /* clear new code status */
					TMR4ON = 0; /* stop counting how long the button is held */
				} else { /* switch 1 pressed */
					led_on(); /* test LED */
					hold = 0; /* reset counter how long the button is held */
					TMR4ON = 1; /* start counting how long the button is held */
				}
			}
			if (switches&SWITCH2) { /* switch 2 changed */
			}
			switches = PORTB; /* save current switch state */
			IOCBF &= ~(SWITCH1|SWITCH2); /* clear switch interrupts */
		}
		IOCIF = 0; /* clear GPIO interrupt */
	}
	if (TMR0IF) { /* timer 0 overflow. no bit within required time */
		TMR0CS = 1; /* stop timer 1 */
		TMR0 = START; /* reset timer */
		TMR0IF = 0; /* clear timer 0 interrupt */
	}
	if (TMR2IF) { /* timer 2 overflow. long pulse */
		TMR2ON = 0; /* stop timer 2 */
		TMR2 = 0xff; /* set timer 2 count to maximum */
		TMR2IF = 0; /* clear timer 2 interrupt */
	}
	if (TMR4IF) { /* timer 4 overflow, 250ms passed during button press */
		hold++; /* increment 250ms counter */
		/* toggle LED */
		if (hold%2) {
			led_off();
		} else {
			led_on();
		}
		/* button pressed for 5s, clear memory */
		if (hold==20) {
			led_on();
			if (!(PORTB&SWITCH1)) { /* ensure the button is pressed */
				clear_memory();
			}
			led_off();
			TMR4ON = 0;
		}
		TMR4IF = 0; /* clear timer 4 interrupt */
	}
}

void main (void)
{
	uint8_t rx = 0; /* are we receiving a pulse */
	uint8_t bit = 0; /* the current received bit */
	uint8_t time = 0; /* time since previous bitframe start, in 0.256ms steps */
	uint16_t rc; /* return code */

	init(); /* configure micro-controller */
	dump_codes(); /* dump codes when powering up so a logic analyzer can get them */

	while (1) { /* a microcontroller runs forever */
		/* I can't go to sleep to safe power
		 * the RADIO is connected to RA0, but port A does not support interrupt on change
		 * the timers are off in sleep, except for timer 1 when externally driven, but this is not our case
		 * the watchdog can only wake up every 1ms, but that is not fast enough and a pulse could be missed
		 */
		if (rx==0 && (PORTA&RADIO)) { /* pulse detected */
			rx = 1; /* mark receiving start to wait until pulse is finished */
			start_pulse_timer();
		} else if (rx!=0 && !(PORTA&RADIO)) { /* end of pulse */
                        rx = 0;
			if (TMR2>=14) { /* only observe pulses >0.9ms */
				/* pulse should be 1ms, but 1.2ms are used in the field

				 * the first transmission can sometimes be detected a 10ms, even with a 1ms pulse
				 * this is why the fallinf edge is used
				 */
				if (bit>0) { /* following pulses */
					time += start_bit_timer(); /* get time and start measure time for next pulse */
					if (time>=27 && time<=35) { /* pulse between 7 and 9 ms after previous bitframe start is a 0 */
						time = 8; /* pulse is 2ms after bitframe start */
						code[bit/8] &= ~(1<<(7-(bit%8))); /* store first bit=0 */
						bit++; /* wait for next bit */
					} else if (time>=40 && time<=47) { /* pulse between 10 and 12 ms after previous bitframe start is a 0 */
						time = 20; /* the sync pulse is 5ms after bitframe start */
						code[bit/8] |= 1<<(7-(bit%8)); /* store first bit=1 */
						bit++; /* wait for next bit */
					} else { /* unexpected pulse. code is broken */
						bit = 0; /* restart from beginning for new code */
					}
					if (bit==24) { /* received all 24 bits */
						led_on(); /* indicate activity */
						rc = save_code(); /* save code in external EEPROM */
						if (!new) { /* only switch led off if no new code has been detected (globally) */
							led_off(); /* activity finished */
						}
						if (rc==1) { /* new code saved */
							new = 1; /* remember a new code has been saved */
							led_on(); /* indicate new code detected */
							LATA |= RELAY1; /* switch relay on to make sound */
							for (rc=0; rc<1024; rc++); /* wait a bit */
							LATA &= ~RELAY1; /* switch relay off */
						}
					}
				}
				if (bit==0) { /* sync pulse (can be the current one it it is not a continuation */
					start_bit_timer(); /* measure time until next bit */
					time = 20; /* the sync pulse is 5ms after bitframe start */
					code[bit/8] |= 1<<(7-(bit%8)); /* store first bit=1 */
					bit++; /* wait for next bit */
				}
			}
		}		
	}
}

