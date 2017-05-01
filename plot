#!/bin/sh

# time rtc lse/2 lse/14 ch1 ch1# ch2 ch2# ch3 ch3# ch4 ch4# int-temp vref vbat
# 1493526556 3.473 2922328755 12441 0 0 2922042558 206 2190027411 154 1997130139 2 92.3259 3.28702 3.09282

grep ^[0-9] typescript | awk '(length($15)) { print }' >samples
./avg --column=15 --combine=30 --id=1 --format=%.6f <samples >vbat.log
./avg --column=14 --combine=30 --id=1 --format=%.6f <samples >vref.log
./avg --every=1 --column=13 --combine=30 --id=1 --format=%.6f <samples >temp.log
./counter-to-ppm --id=1 --column=7 --frequency=48000000 --number=8 --limit=10 <samples | awk '(length($4) && $1 >= 1493592110) { print }' | ./avg --every=1 --column=4 --combine=30 --id=1 --format=%.3f >hse.log
./counter-to-ppm-adjust16 --column=3 --adjust16=4 --id=1 --frequency=48000000 <samples | awk '($5 < 3 && $5 > -3 && $6 <100 && $6 >60) { print }' | ./avg --every=1 --column=6 --combine=30 --id=1 --format=%.3f >lse.log
join -j1 temp.log hse.log >temp-hse.log
join -j1 temp-hse.log lse.log >temp-lse.log
join -j1 temp.log vbat.log >temp-vbat.log

# board1 - 1750 [1742 1314] 1527 [1520] 1918 3358
# board2 - 1754 [1754 1324] 1525 [1521] 1895 3273

gnuplot <<EOF
set terminal png size 900,600
set xdata time
set timefmt "%s"
set output "vbat.png"
set grid
set xlabel "DD-HH:MM"
set format x "%d-%H:%M"
set xtic rotate by -45 scale 0
set ylabel ""
set ytics format "%1.4f V" nomirror
set title "coin cell battery"
set key bottom left box
plot \
'vbat.log' using 1:3 title 'vbat' with lines
EOF

gnuplot <<EOF
set terminal png size 900,600
set xdata time
set timefmt "%s"
set output "vref.png"
set grid
set xlabel "DD-HH:MM"
set format x "%d-%H:%M"
set xtic rotate by -45 scale 0
set ylabel ""
set ytics format "%1.4f V" nomirror
set title "voltage reference"
set key bottom left box
plot \
'vref.log' using 1:3 title 'vref' with lines
EOF

gnuplot <<EOF
set terminal png size 900,600
set xdata time
set timefmt "%s"
set output "temp.png"
set grid
set xlabel "DD-HH:MM"
set format x "%d-%H:%M"
set xtic rotate by -45 scale 0
set ylabel ""
set ytics format "%1.2f F" nomirror
set title "temperature sensor"
set key bottom left box
plot \
'temp.log' using 1:3 title 'temp' with lines
EOF

gnuplot <<EOF
set terminal png size 900,600
set xdata time
set timefmt "%s"
set output "lse.png"
set grid
set xlabel "DD-HH:MM"
set format x "%d-%H:%M"
set xtic rotate by -45 scale 0
set ylabel "offset"
set ytics format "%1.3f ppm" nomirror
set title "32khz RTC"
set key bottom left box
plot \
'lse.log' using 1:3 title 'lse' with lines, \
'temp-lse.log' using 1:(\$7-\$5) title 'lse-hse' with lines
EOF

gnuplot <<EOF
set terminal png size 900,600
set xdata time
set timefmt "%s"
set output "hse.png"
set grid
set xlabel "DD-HH:MM"
set format x "%d-%H:%M"
set xtic rotate by -45 scale 0
set ylabel "offset"
set ytics format "%1.3f ppm" nomirror
set title "12MHz TCXO"
set key bottom left box
plot \
'hse.log' using 1:3 title 'hse' with lines
EOF

gnuplot <<EOF
a=-0.888968
b=-0.0206779
c=0.00030986
d=1.98162
f(x) = a+b*(x-d)+c*(x-d)**2
fit f(x) "temp-hse.log" using 3:5 via a,b,c,d
fit_stddev = sqrt(FIT_WSSR / (FIT_NDF + 1 ))

set label 1 gprintf("fit RMSE = %1.3f ppm", fit_stddev) at graph 0.9,0.9 right front

set terminal png size 900,600
set output "temp-hse.png"
set grid
set xlabel "Temperature"
set format x "%1.1f F"
set xtic rotate by -45 scale 0
set ylabel "frequency"
set ytics format "%1.3f ppm" nomirror
set title "12MHz TCXO"
set key bottom right box
plot \
'temp-hse.log' using 3:5 title 'hse' with points, \
 f(x) title "temp poly fit" with line, \
 f(x)+fit_stddev title "poly fit + RMSE" with line, \
 f(x)-fit_stddev title "poly fit - RMSE" with line
EOF

gnuplot <<EOF
a=78.678
c=-0.008641975
d=86

f(x) = a+c*(x-d)**2
fit f(x) "temp-lse.log" using 3:(\$7-\$5) via a,c,d
fit_stddev = sqrt(FIT_WSSR / (FIT_NDF + 1 ))

set label 1 gprintf("fit RMSE = %1.3f ppm", fit_stddev) at graph 0.9,0.9 right front

set terminal png size 900,600
set output "temp-lse.png"
set grid
set xlabel "Temperature"
set format x "%1.1f F"
set xtic rotate by -45 scale 0
set ylabel "frequency"
set ytics format "%1.3f ppm" nomirror
set title "32khz RTC"
set key bottom left box
plot \
'temp-lse.log' using 3:(\$7-\$5) title 'lse' with points, \
 f(x) title "temp poly fit" with line, \
 f(x)+fit_stddev title "poly fit + RMSE" with line, \
 f(x)-fit_stddev title "poly fit - RMSE" with line
EOF

gnuplot <<EOF
set terminal png size 900,600
set output "temp-vbat.png"
set grid
set xlabel "Temperature"
set format x "%1.1f F"
set xtic rotate by -45 scale 0
set ylabel "Voltage"
set ytics format "%1.4f V" nomirror
set title "Coin Cell"
set key bottom left box
plot \
'temp-vbat.log' using 3:5 title 'vbat' with points
EOF

rsync -avP *.png index.html vps3:dan.drown.org/html/stm32/hat2/
