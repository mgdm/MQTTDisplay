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


