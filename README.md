This project is about the Linear's gate control system MegaCode.
Product page: http://www.linearcorp.com/radio_control.php#megacode

The transmission protocol has been reversed, and the firmware for the remote and receiver rewritten.
It can now be used to send and receive custom codes.

megacode
========

The transmission protocol works as follows:

The value is encoded using Linear LLC MegaCode scheme.
It uses AM/ASK/A1D pulse position for the radio signal.

The radio transmission uses the 318MHz frenquency.
The signal is encoded using Linear LLC MegaCode scheme.
It uses AM/ASK/A1D pulse position for the radio signal.
24 bits are transmitted:
- 1 sync bit
- 16 bits for the remote code
- 4 bits for the facility code
- 3 bits for data bits (the channel/button used)

24 bursts are transmitted, plus 1 blank burst, within 150 ms.
Each burst is a 6 ms bitframe.
Withing the burst there is a 1 ms pulse after 2 or 5 ms.
The blank burst does not include a pulse and is used to separate transmissions.
The first pulse is used to synchronize and is always after 5ms within the burst.

sdr
===

This folder contains tools to be used with Software Defined Radio (SDR).

A RTL-SDR has been used to capture the signal.
Use *sdrangelove* to figure out the frequency.
It is around 318MHz, but +/- 100kHz.
Use *rtl_fm* to record the transmission:
	rtl_fm -f 317.962M -M am megacode.pcm
A few remote transmissions have been captured and the recordings are saved in *samples*

*megacode.pcm* will have signed 16 bits little endian sample, at 24000Hz.
Use *decode.rb* to decode this recording:
	./decode.rb megacode.pcm

To record is an opportunistic way (someone uses an unknown remote further away), you have to tweak *rtl_fm*:
	rtl_fm -f 317.9M:318.1M:20k -g 10 -l 700 -M am megacode.pcm

pic
===

This folder contains firmwares for the transmitter and receiver micro-controllers.

318LIPW1K
---------

The Monarch 318LIPW1K(-L) is a compatible/clone of the Linear ACT-31B.
Product page: http://www.communitycontrols.com/Product/?PID=196
Manual: http://s3.amazonaws.com/CommunityControls/PDFs/CC-Monarch-318LIPw1K.pdf
FCCID: SU7318LIPW1K

It uses a re-programmable chip (flash based).
It uses a PIC12F629 (SN package) micro-controller.
Monarch also advertises that the code is programmable.
I could not find the software.
The programming header is even present on the board.
But the micro-controller has read protection enabled.

The firmware has been re-implemented from scratch.
It can send custom codes.
Either change the default code in the *318LPW1K-L.c* source code and reflash the device:
	make flash
Or change the code it in the *eeprom.asm* file and only rewrite the EEPROM:
	make eeprom
Then press the button to send your code.

ACT-34B
-------

The Linear ACT-34B is a gate remote.
Product page: http://www.linearcorp.com/product_detail.php?productId=867
Manual: http://www.linearcorp.com/pdf/manuals/ACT-31B_ACT-34B.pdf
FCCID: EF4ACP00872

The PCB of the ACT-31B is the same than the ACT-34B, with only one switch out of four populated.
It uses a PIC12C508A (SM package) micro-controller.
This micro-controller is EEPROM based (designated by the 'C' in the name) which the PICkit 2 can't program.
It is programmed using the test points, but only once since it's a One Time Programmable (OTP) chip.

A pin compatible flash based chip can be used instead.
Most P12FXXX are, like the PIC12F629/PIC12F675 (simplest alternative), PIC12F617 (more flash but no EEPROM), and PIC12F1840 (high end).
They come in SN packages, which is thiner then the original SM package.
But the pitch is the same and the pins can be soldered on the pads.

The firmware can be flashed on a PIC12F617 using:
	*make all*
The code is hard coded in the *ACT-34B.c* source file.
Button functions:
- big top right button: send a primary code
- red bottom right button: send a secondary code (e.g. security code to open all doors)
- smaller top left button: start sending random codes
- black bottom left button: stop sending random codes

MDR

MDR
---

The MDR is a MegaCode receiver which can activate the gate motors if the right code is received.
Product page: http://www.linearcorp.com/product_detail.php?productId=941
Manual: http://www.linearcorp.com/pdf/manuals/MDR_MDR-2_MDRM.pdf

Codes can be programmed in the receiver by pressing the "learn" button and activating the remote, which will transmit the signal.
The device requires a 24V power source, but it can go down to 17V.

The board uses only trough hole components, and is one-sided.
This makes it very easy to trace path, measure a different points and pin, and exchange parts.
The MDR only has 1 "channel", while the MDR-2 has 2.
The board is the same.
The only hardware difference is that the MDR has a switch and a relay which are not populated.
The main different is that the MDR-2 can allow 2x10 codes to be programmed instead of 10.
But this is only an artificial software limitation.

A PIC16C54A micro-controller is used to provide the main function.
A TLC555CP timer is used as clock to match the 318MHz frequency on which the codes are transmitted.
A LM358N opamp is used to get the pulses out of the received signal.
A 24LC254 I²C EEPROM is used to store which code is allowed.
It can not be programmed in circuit because a pull-up resistor is missing on SCL.
The PIC implement I²C as software bit banging by driving the pins (not using open collectors).
A LM78L05ACZ voltage regulator will provide 5V for the logic, except the relay.
The PICkit2 does not provide enough current to power the logic, but a USB port does.

The MDR-U is a more compact and semi-SMT version of the MDR.
Product page: http://www.linearcorp.com/product_detail.php?productId=942
Manual: http://www.linearcorp.com/pdf/manuals/MDRU.pdf

It plugs in a wall socket instead of requiring 24V.
Beware, the neutral blade is used as GND on the board.
Do not connect an oscilloscope earth to the PCB ground as you will short the mains.
Either use the difference between two oscilloscope probes, or provide yourself 5V (e.g. on the voltage regulator pins)

A PIC16F1847 replaces the original PIC16C54A to help the custom firmware.
To flash it (in circuit and with external power) use:
	make all
The firmware saves the MegeCodes codes it receives in EEPROM.
The codes are read out when the MDR is powered up so a logic analyzer can capture them.
The unique codes are stored as 3 bytes one behind each other.
It can same 256 (kb) x 1024 (b/kb) / 8 (b/B) / 3 (B/code) =10922 codes.
Every time a code is received, the LED blinks.
If the code is new the LED stays on.
Switch the LED off by pressing the button.
Hold the button for 5s (it will blink 20 times) to clear the EEPROM from all codes.

eeprom
======

This folder contains traces from the MDR I²C communication.

The bus pirate was not able to sniff the complete traffic.
I used hardware version 3.6 sparkfun 2/11/2010 with software version 6.1.
The last byte is probably not detected because the stop condition is missing.

I used a Saleae Logic 16 logic analyzer to monitor the traffic.
	sigrok-cli --driver saleae-logic16 --output-format hex --channels 0,1 --protocol-decoders i2c:sda=0:scl=1 --config samplerate=1M --continuous | grep ":"

The EEPROM contains the programmed/learned codes which will activate the relay.
If the code 0xABCDEF is tranmitted, the microcontroller with read the byte at address 0x(B&7)ECD.
The bits in this byte will tell which even value D is authorized (odd values are rounded down).
If the byte = 0x01, only F = 0x0 (and 0x1) is authorized.
If the byte = 0x02, only F = 0x2 (and 0x3) is authorized.
...
If the byte = 0x80, only F = 0xe (and 0xf) is authorized.
If the byte = 0x03, only F = 0x0, 0x1, 0x2, and 0x3 are authorized.
If the byte = 0x07, only F = 0x0, 0x1, 0x2, 0x3, 0x4, 0x5 are authorized.

This is quite clever as the ID is used as address.
It saves space, although there is by for too much available.
It also increases the checking speed as it does not need to go through the memory.
To verify how many codes have been saved, the whole memory is read once in the beginning and the number of codes counted (to enforce the software limit).
****