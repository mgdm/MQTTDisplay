# MQTTDisplay

This displays MQTT messages on a Sure Electronics LCD display. It requires my
[libsureelec](https://github.com/mgdm/libsureelec) library to speak to the
hardware, and [libmosquitto](http://mosquitto.org) to handle the MQTT
connection.

## Usage

`mqttdisplay -b BROKER -p PORT -d DISPLAY`

BROKER is the hostname of the MQTT broker. Defaults to localhost. PORT is the
MQTT port, and defaults to 1883. DISPLAY is the display device, and defaults to
`/dev/tty.SLAB_USBtoUART`, which is correct for the CP2102 driver on Mac OS X.

You can store the options in a config file, which is currently hard-coded to be
`~/.mqttdisplay`. For example:

```ini
[mqttdisplay]
broker=mqtt.example.org
port=1883
topic=test/example
```

## Credits
This code uses the INIH library for parsing the config file.
https://code.google.com/p/inih/

I also stole the trick to avoid the warning about daemon() being deprecated
from mDNSResponder:

http://www.opensource.apple.com/source/mDNSResponder/mDNSResponder-333.10/mDNSPosix/PosixDaemon.c
