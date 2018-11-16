# OTA Update ESP32 Arduino
This repository is used to test a http OTA update on the ESP32 in Arduino Backend.

I've used this example as base for this code: https://github.com/espressif/arduino-esp32/tree/master/libraries/Update/examples

What's the difference between the official example and this code? Well, the example uses the Updater class which is designed to work with a FileStream object. If we put in a WiFiClient stream, sometimes it throws an timeout exception. This is because the WiFiClient is dependent on external factors like a stable network connection. If there's a hickup in the connection, this example does not work.

I fixed this by caching the update first to a SD card, and finaly copy it over to the flash chip of the ESP32.

# What is needed?
* Fileserver somewhere
* Upload json text-file to this fileserver with the following content:
`
{
    "version": 0.2,
    "file": "http://ferryjongmans.nl/esp/bikeshift-node-V_02.bin"
}
`
This helps the ESP to check for new updates. And if so, it provides the url of the binary firmware file.
