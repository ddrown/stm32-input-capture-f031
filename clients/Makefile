CFLAGS=-Wall -std=gnu11
CC=gcc

all: set-calibration-data ds3231 pcf2129 rtc calibration input-capture-i2c dump-adc timestamps-i2c
# timestamps-i2c timestamps-gpio pi-pwm-setup

input-capture-i2c: input-capture-i2c.o i2c.o timespec.o i2c_registers.o adc_calc.o vref_calc.o avg.o float.o aging.o tempcomp.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

frequency: frequency.o i2c.o i2c_registers.o float.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

dump-adc: dump-adc.o i2c.o i2c_registers.o float.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

timestamps-i2c: timestamps-i2c.o i2c.o i2c_registers.o timespec.o float.o adc_calc.o vref_calc.o tempcomp.o avg.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

timestamps-gpio: timestamps-gpio.o i2c.o i2c_registers.o timespec.o
	$(CC) $(CFLAGS) -o $@ $^ -lwiringPi

set-calibration-data: set-calibration-data.o i2c.o i2c_registers.o float.o
	$(CC) $(CFLAGS) -o $@ $^

rtc: rtc.o i2c.o i2c_registers.o rtc_data.o float.o aging.o
	$(CC) $(CFLAGS) -o $@ $^

pi-pwm-disable: pi-pwm-disable.o
	$(CC) $(CFLAGS) -o $@ $^ -lwiringPi -pthread

pi-pwm-setup: pi-pwm-setup.o
	$(CC) $(CFLAGS) -o $@ $^ -lwiringPi -pthread

ds3231: ds3231.o i2c.o
	$(CC) $(CFLAGS) -o $@ $^

pcf2129: pcf2129.o i2c.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

calibration: calibration.o i2c.o i2c_registers.o adc_calc.o vref_calc.o avg.o rtc_data.o float.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

dump: dump.o i2c.o
	$(CC) $(CFLAGS) -o $@ $^

offset: offset.o i2c.o i2c_registers.o
	$(CC) $(CFLAGS) -o $@ $^

poll: poll.o i2c.o
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: all
