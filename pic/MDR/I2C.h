#define __16f1847
#include <pic16f1847.h>
#include <stdint.h>
 
#define SCL _RB6 /* pin 12, external 24LC256 I2C EEPROM memory */
#define SDA _RB7 /* pin 13, external 24LC256 I2C EEPROM memory */

void send_start(void);
void send_stop(void);
uint8_t send_byte(uint8_t byte);
uint8_t read_byte(uint8_t ack);
