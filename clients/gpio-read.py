#!/usr/bin/env python3

from pathlib import Path
from time import sleep, time

# line 102: "J2 Header Pin12"
input = 480
gpiovalue = Path(f"/sys/class/gpio/gpio{input}/value")

if not gpiovalue.is_file():
    with open("/sys/class/gpio/export","w") as export:
        export.write(str(input))

with gpiovalue.open() as value:
    for sec in range(0,60):
        v = ""
        for i in range(0,100):
            value.seek(0)
            v += value.read().rstrip()
            sleep(0.01)
        print(f"{int(time())} {v}")

with open("/sys/class/gpio/unexport","w") as export:
    export.write(str(input))
