#!/bin/sh
# Set up I/O using WiringPi
# Call this once before running vbit
# But if we run as root much of this isn't needed.
#
# LED on pin 7 (GPIO4) wpi =7
/usr/local/bin/gpio mode 7 out
# FLD on pin 15 (GPIO22) wpi=3, This interrupt drives the whole system
/usr/local/bin/gpio edge 3 none
# CSN on pin 24: wpi=10
#gpio mode 10 out
# MUX on pin 22 is WiringPi 6
/usr/local/bin/gpio mode 6 out
# Now sort out the I2C (alias TWI)
/usr/local/bin/gpio load i2c
/usr/local/bin/gpio i2cdetect
# And finally the SPI
/usr/local/bin/gpio load spi
# and even more finally, On Rasbian CE0 is not really controllable
# so we need to use general I/O as chip select
/usr/local/bin/gpio mode 5 out

