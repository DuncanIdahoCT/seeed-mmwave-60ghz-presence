# seed-mmwave-60ghz-presence
A working MQTT based Home Assistant Integration using the Seed Studio MR60FDA1

![Arduino IDE](/static/images/Arduino%20IDE.png)

This project is a bit of a work in progress but I have it up and running and the sensor state is now stable and reliable in Home Assistant:

![Home Assistant Sensor Entity](/static/images/home%20assistant%20sensor%20entity.png)

# Why MQTT?

At first I thought to use the sensor with an extra ESP32 I had laying around but as much as I tried to tinker with it, I could not make it work using ESPHome. However, there is an "okay" example code set for Arduino IDE... a sample sketch... referenced on the Seeed Studio Wiki:

https://wiki.seeedstudio.com/Radar_MR60FDA1/
https://github.com/limengdu/Seeed-Studio-MR60FDA1-Sersor

Their code is a bit odd... but it hinted at things and I managed to figure out how to use an ESP32 in Arduino IDE and then worked out that Serial 2 is what I want, not SoftwareSerial as many other threads suggested... the ESP32 has 3 actual hardware serial ports but only Serial 2 is "readily" usable from what I understood. As soon as I modified the code to use Serial 2 and set the pins to my preference the sensor started outputting data to the Arduino IDE serial monitor.

I then added code examples for WiFi and MQTT so I could send sensor data to Home Assistant the only way I knew would work given that I could not manage to get the code to work in ESPHome.

This is what it looks like at the moment but it works 100%:

![Project Breadboard](/static/images/Seeed%2060Ghz%20mmWave%20-%20ESP32.jpg)

# Integrating into Home Assistant

