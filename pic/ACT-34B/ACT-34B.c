/* micro-controller firmware fot the Linear ACT-34B remote
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
#define __12f617
#include <pic12f617.h>
#include <stdint.h>

/* the peripherals connected to the pins */
#define SWITCH1 _GP0
#define SWITCH2 _GP3
// note: SWITCH3 will behave as pressed when SWITCH4 is pressed
#define SWITCH3 _GP1
#define SWITCH4 _GP2
// note: CLOCK will be enable also if only TX is enabled
#define CLOCK _GP5
#define TX _GP4

/* simple functions */
#define clock_on() GPIO |= CLOCK;
#define clock_off() GPIO &= ~CLOCK;
#define tx_on() GPIO |= TX;
#define tx_off() GPIO &= ~TX;
#define sleep() __asm sleep __endasm

/* variables */
// a megacode is 3 bytes long (MSB of byte 1 is 1)
# define CODE 0xc917c2
// transmitting (0: do not transmit, 1: transmit, -1: finish transmiting, -2: pause before next transmission)
volatile int8_t transmit = 0;
// the number or timer 2 overflow (0x06554s) to wait before next transmission
volatile int8_t pause = 0;
 // the 4 phases of the bit (2ms pause, 1ms pulse, 2ms pause, 1ms pulse. only transmit during one of the two pulse periode depending on the bit value)
volatile uint8_t phase = 0;
// save the GPIO state to be able to figure out which changed
volatile last_gpio = GPIO;

/* configuration bits */
uint16_t __at(_CONFIG) __CONFIG = _WRT_OFF & // entire memory write protected
                                  _BOREN_OFF & // brown-out reset off
                                  _IOSCFS_4MHZ & // set internal oscillator to 4MHz
                                  _CP_OFF & // no code protection
                                  _MCLRE_OFF & // disable master clear reset
                                  _PWRTE_ON & // enable power-up timer
                                  _WDTE_OFF & // disable watchdog
                                  _INTRC_OSC_NOCLKOUT; // use internal oscillator and both I/O pins

void timer_1ms() {
  TMR1ON = 0; // disable timer 1 (to write value safely)
  TMR1L = 0x88; // set time (tuned per hand)
  TMR1H = 0xff; // set time
  TMR1ON = 1; // start timer 1
}

void timer_2ms() {
  TMR1ON = 0; // disable timer 1 (to write value safely)
  TMR1L = 0x23; // set time (tuned per hand)
  TMR1H = 0xff; // set time
  TMR1ON = 1; // start timer 1
}

/* transmit the megacode */
void megacode (void) {
  uint8_t bit = phase/4;
  if (transmit != 0) {
    if (bit<24) { // transmit bit
      if (phase%2) {
        uint8_t pulse = (CODE>>(23-bit))&0x01;
        if ((phase%4==1 && !pulse) || (phase%4==3 && pulse)) {
          tx_on();
        }
        timer_1ms();
      } else {
        tx_off();
        timer_2ms();
      }
    } else if (bit==24) { // 25th bit is a blank
      tx_off();
      timer_2ms();
    } else { // restart after 25th bit
      phase = 0xff; // phase will be 0 after incrementing
      clock_off(); // stop clock
      if (transmit == -1) { // stop transmiting if requested
        transmit = 0; // stop transmiting
      } else {  // pause before next transmission
        transmit = -2; // pause state
        TMR2 = 0; // clear timer 2 (used to measure timer)
        pause = 0; // reset number of timer 2 overflows (65.54ms)
        TMR2ON = 1; // start timer 2
      }
    }
    phase++; // go to next phase
  }
}

/* initialize micro-conroller */
void init (void) {
  ANSEL = 0; // all pins are digital
  TRISIO |= SWITCH1|SWITCH2|SWITCH3|SWITCH4; // switches are inputs (per default)
  TRISIO &= ~(CLOCK|TX); // all other are outputs
  NOT_GPPU = 0; // enable global weak pull-up for inputs
  WPU |= SWITCH1|SWITCH3|SWITCH4; // use internal pull-up on switches (the external are weak)
  // SWITCH2 pull-up is not enabled because pins is not used as MCLR
  GPIO &= ~(CLOCK|TX); // clear output

  IOC |= (SWITCH1|SWITCH2|SWITCH3|SWITCH4); // enable interrupt for the switches
  GPIE = 1; // enable interrupt on GPIO
  last_gpio = GPIO; // save current state

  /* use precise timer 1 for the megacode transmission timing */
  T1CON = _T1CKPS1 | _T1CKPS0; // set prescaler to 8
  // 0 is set per default, but just be sure
  TMR1ON = 0; // stop timer 1
  TMR1GE = 0; // enable timer 1
  TMR1CS = 0; // use internal clock / 4 (=1MHz)
  TMR1IF = 0; // clear interrupt
  PIE1 = 1; // enable timer 1 interrupt
  PEIE = 1; // enable timer interrupt

  /* use timer 2 to separate two transmissions in time */
  TMR2ON = 0; // switch off timer 2
  T2CKPS0 = 1; // set prescaler to 16
  T2CKPS1 = 1; // set prescaler to 16
  TOUTPS0 = 1; // set postscaler to 16
  TOUTPS1 = 1; // set postscaler to 16
  TOUTPS2 = 1; // set postscaler to 16
  TOUTPS3 = 1; // set postscaler to 16
  PR2 = 0xff; // set interrupt on overflow
  TMR2 = 0; // clear timer counter
  TMR2IE = 1; // enable timer 2 interrupt
  TMR2IF = 0; // clear timer 2 interrupt

  GIE = 1; // enable interrups

}

// funcion called on interrupts
// interrupt 0 is only one on PIC12
static void interrupt(void) __interrupt 0
{
  if (GPIF) { // pin state changed
    last_gpio ^= GPIO; // figure out which GPIO changed
    if (last_gpio&SWITCH4) { // switch 1 (bigger button on top right) changed
      if (GPIO&SWITCH4) { // button released, stop transmission
        if (transmit != 0) { // currently transmitting
          transmit = -1; // stop transmitting after last transmition
        }
      } else { // switch is pressed, start transmission
        if (transmit == 0) { // only transmit if code is loaded
          transmit = 1; // start transmission procedure
          phase = 0; // start from beginning
          clock_on(); // start clock and leave it on during the whole transmission
          megacode(); // start sending megacode
        }
      }
    }
    last_gpio = GPIO; // save current state
    GPIF = 0; // clear interrupt
  }
  if (TMR1IF) { // timer 1 overflow, used to time megacode pulses
    megacode(); // continue sending megacode
    TMR1IF = 0; // clean interrupt
  }
  if (TMR2IF) { // timer 2 overflow, used to pause between two transmissions
    pause++; // number of timer 2 overflows (65.54ms)
    if (transmit == -1) { // stop transmission
      TMR2ON = 0; // stop counting pauses
      transmit = 0; // stop transmission
    } else if (pause==4) { // time between transmissions passed
      transmit = 1;
      phase = 0; // start from beginning
      clock_on(); // start clock and leave it on during the whole transmission
      megacode(); // start sending megacode
      TMR2ON = 0; // stop counting pauses
    }
    TMR2IF = 0; // clean interrupt
  }
}

void main (void)
{
  init(); // configure micro-controller
  transmit = 0; // disable transmission initialy

  while (1) { // a microcontroller runs forever
    if (transmit == 0) {
//      sleep(); // sleep to save power. this will shut down timer 1, but button press will wake it up
    }
  }
}
