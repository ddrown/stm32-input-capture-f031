This was originally designed to accept a very low frequency pwm output (1-50Hz) and measure the SBC's frequency vs the TCXO's. That frequency data can then be fed into chrony's temperature compensation system to result in more stable time for NTP service.

Hardware is a custom designed SBC hat with microcontroller and TCXO

The TCXO's frequency vs temperature curve is measured and stored in flash in order to increase long term frequency accuracy.

The hat also contains an RTC with sub-second precision. Usually i2c RTCs will only tell you time to the nearest second, so you have to poll them to find out when the second changes. The RTC runs off the 32khz crystal, but can be measured in hardware against the TCXO with `clients/rtc compare`

Timers used
* TIM2 - main 32 bit timer, all 4 channels are setup as input capture
* TIM14 - 16 bit timer, measures RTC vs TCXO on channel 1

settings in raspi-config:

i2c enabled
(optional for gps) serial login shell disabled, serial interface enabled

/boot/config.txt:
dtparam=i2c\_arm=on
dtparam=i2c\_arm\_baudrate=400000
\# optional for gps
dtoverlay=pps-gpio,gpiopin=13

stm32 source code:

* Src/system_stm32f0xx.c - bootup, start clocks
* Src/main.c - init peripherals, main loop
* Src/stm32f0xx_hal_msp.c - init irq, init gpio routing, init clocks, etc
* Src/stm32f0xx_it.c - irq handlers (mostly call into HAL)
* Src/adc.c - measure temperature, vcc, and rtc battery
* Src/flash.c - store TCXO parameters in flash
* Src/i2c_slave.c - i2c interface to talk to SBC
* Src/timer.c - manages TIM2, TIM14, and RTC
* Src/uart.c - debug uart interface
* Drivers/STM32F0xx_HAL_Driver - stm32 Hardware Abstraction Layer (HAL)

SBC tool code:

* clients/timestamps-i2c.c - compare SBC system time vs TCXO, request via i2c
* clients/input-capture-i2c.c - compare SBC system frequency vs TCXO, via SBC output pwm
* clients/pi-pwm-setup.c - wiringPi setup SBC output pwm
* clients/pi-pwm-disable.c - wiringPi disable SBC output pwm
* clients/odroid-c2-setup - sysfs setup SBC output pwm
* clients/rtc.c - read/set/sync/manage rtc
* clients/set-calibration-data.c - get/set TCXO calibration data in flash
* clients/ds3231.c - DS3231 RTC i2c client
* clients/pcf2129.c - PCF2129 RTC i2c client
* clients/calibration.c - measure RTC vs TCXO
* clients/dump-adc.c - test comparing floating vs fixed point math

SBC library code:

* clients/adc_calc.c - read ADC data over i2c
* clients/aging.c - estimate TCXO frequency from aging effects
* clients/avg.c - calculate averages
* clients/float.c - convert between native float and i2c compatible
* clients/i2c.c - i2c interface functions
* clients/i2c_registers.c - read stm32's exported data via i2c
* clients/rtc_data.c - convert between RTC data and system time
* clients/tempcomp.c - calculate TCXO's temperature compensation
* clients/timespec.c - struct timespec utilities
* clients/vref_calc.c - adc voltage calculations

Programming the stm32 can be done via openocd on the connected SBC.

SWD programming interface is connected to SBC pins:
* 18 - dio
* 22 - clk
* 11 - rst

Example openocd configuration:
* openocd.cfg - raspberry pi & stlink example
* openocd-odroid.cfg - gpio sysfs on odroid-c2 SBC
