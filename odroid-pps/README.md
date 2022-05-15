$ sudo make install
cpp -nostdinc -I . -undef pps-pin12.dts pps-pin12.dts.preproc
dtc -@ -O dtb -o pps-pin12.dtbo pps-pin12.dts.preproc
mkdir -p /boot/overlay-user/
cp pps-pin12.dtbo /boot/overlay-user/

/boot/armbianEnv.txt: `user_overlays=pps-pin12`
