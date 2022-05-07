#!/usr/bin/env python3

from pathlib import Path
from time import sleep, time
import sys

def find_first_second(value):
    for sec in range(0,1000):
        value.seek(0)
        if value.read(1) == "1":
            now = time()
            sleep(0.995)
            return now
        sleep(0.001)
    return None

# line 102: "J2 Header Pin12"
input = 480
gpiovalue = Path(f"/sys/class/gpio/gpio{input}/value")

if not gpiovalue.is_file():
    with open("/sys/class/gpio/export","w") as export:
        export.write(str(input))

with gpiovalue.open() as value:
    previous = find_first_second(value)
    while True:
        value.seek(0)
        if value.read(1) == "1":
            now = time()
            print(f"{now} {now-previous}")
            sys.stdout.flush()
            sleep(0.995)
        previous = time()

with open("/sys/class/gpio/unexport","w") as export:
    export.write(str(input))
