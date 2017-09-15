settings in raspi-config:

i2c enabled
(optional for gps) serial login shell disabled, serial interface enabled

/boot/config.txt:
dtparam=i2c\_arm=on
dtparam=i2c\_arm\_baudrate=400000
\# optional for gps
dtoverlay=pps-gpio,gpiopin=13
