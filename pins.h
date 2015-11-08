/** pins.h
 * These are WiringPi numbers, not physical or Broadcom.
 * Pin mappings for Raspberry Pi GPIO
 * FLD = Video Field changes every field
 * MUX = Sets the clock source of the serial ram High=VBIT Low=CPU
 * LED = High switches on LED
 * CSN = High=deselects serial RAM. Low selects serial RAM.
 * (RPi: CE0 releases the slave after each block. We need it to stay connected)
 */
#ifndef _PINS_H_
#define _PINS_H_
#define GPIO_FLD 3
#define GPIO_CSN 5
#define GPIO_MUX 6
#define GPIO_LED 7
#endif
