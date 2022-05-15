pps-pin12.dtbo: pps-pin12.dts
	cpp -nostdinc -I . -undef $^ $^.preproc
	dtc -@ -O dtb -o $@ $^.preproc

install: pps-pin12.dtbo
	mkdir -p /boot/overlay-user/
	cp pps-pin12.dtbo /boot/overlay-user/
.PHONY: install
