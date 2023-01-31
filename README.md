# seeed-mmwave-60ghz-presence
A working MQTT based Home Assistant Integration using the Seeed Studio MR60FDA1

Presence with Motion:

![Working Sensor in Action](/static/images/presence+motion.png)

Stationary Presence:

![Working Sensor in Action](/static/images/presence+stationary.png)

# Humble Beginnings

I started out just being able to see the sensor messages in Arduino IDE, and I only had an ESP32 to use as the MCU so it took a bit of tinkering to even get started:

![Arduino IDE](/static/images/Arduino%20IDE.png)

Next I decided to try to get a basic sensor entity working in Home Assistant using manual configuration.yaml sensor settings with a state topic I could update using messages from the sensor example code:

![Basic Sensor Entity](/static/images/home%20assistant%20sensor%20entity.png)



I've updated the code to use MQTT Discovery, so that it will create an actual device with multiple sensors/binary_sensors that will eventually show the various states that are tracked by this sensor:

# Why MQTT?

At first I thought to use the sensor with an extra ESP32 I had laying around but as much as I tried to tinker with it, I could not make it work using ESPHome. However, there is an example code set for Arduino IDE... a sample sketch... referenced on the Seeed Studio Wiki:

https://wiki.seeedstudio.com/Radar_MR60FDA1/

https://github.com/limengdu/Seeed-Studio-MR60FDA1-Sersor
						    ^ <--not a typo, there is a misspelling in the repo name, but hey, I misspelled Seeed when I first wrote this ;)

Their code is a bit hard for me to follow, but it hinted at things and I managed to figure out how to use an ESP32 in Arduino IDE and then worked out that Serial 2 is what I want, not SoftwareSerial as many other threads suggested... the ESP32 has 3 actual hardware serial ports but only Serial 2 is "readily" usable from what I understood. As soon as I modified the code to use Serial 2 and set the pins to my preference the sensor started outputting data to the Arduino IDE serial monitor.

I then added code examples for WiFi and MQTT so I could send sensor data to Home Assistant the only way I knew would work given that I could not manage to get the code to work in ESPHome.

This is what it looks like at the moment, but it works 100%:

![Project Breadboard](/static/images/Seeed%2060Ghz%20mmWave%20-%20ESP32.jpg)

# Integrating into Home Assistant

Once you flash the device, and ensuring you have an MQTT broker setup in HA, you only need to turn it on and connect to your WiFi, it will send configuration topic messages to MQTT in a format that are recognized as MQTT Discovery which will create the device and populate it with sensors and with any luck, you'll see state messages

This is no longer necessary, if done already, it should be removed and HA restarted or the sensors config reloaded from Development Tools:
```
  # Example configuration.yaml entry
  mqtt:
    sensor:
      - name: "mmwave-bedroom-60ghz-presence"
        state_topic: "mmwave/bedroom"
        unique_id: [use a GUID generator for this]
```

And of course using Arduino IDE you'll need to flash the code from the seed-mmwave-60ghz-presence.ino file on this repo

You may also need to add packages in Arduino IDE for these includes to work:
```
  #include <ArduinoJson.h>
  #include <ArduinoMqttClient.h>
  #include <WiFi.h>
```
Note: if you're not using an ESP32 of some type, you'll need to change this ifdef statement:

```
  #ifdef ESP32
	  #define RXD2 23
	  #define TXD2 5
  #endif
```
The code should of course be fully reviewed before use, all passwords and IPs/Hostnames are cleared out and ssid and wifi passwords should probably be in a secrets file, but I was being lazy and this is just a sandbox project... I still need to work out how to send settings to the Seeed 60Ghz sensor to reduce its range as right now it shoots right through the wall and picks up motion in the hallway.

When you compile/upload the code to your Arduino of choice or an ESP32 like I did, there is a lot of extra debug message action that I've setup so you can see how it's working. Tune your Arduino IDE Serial Monitor to 115200 baud to see this, if you're quick about it after it is done uploading and the Arduino/ESP is rebooting, you'll see the initial WiFi join and then the start up and connection status of the MQTT sender(client)

# A note on using MQTT for a real-time sensor:

I found that it tended to disconnect from MQTT after any long period of inactivity with respect to the mmWave sensor, at first I tried using MQTT polling to keep this connection alive but this didn't work well for a couple of reasons, 1) I could not come up with a simple way to poll "now and then" other than writing in a silly counter to the void loop and 2) even with that, it seemed unnecessary and dirty to be polling every X loops. So I took from an example I found to separate out the connection and any subsequent reconnection to a void function that's called upon void setup and on each void loop but all it does is check to see if "mqttClient.connected" and if it is, it "returns;" and so adds no unnecessary network activity or polling. See example below:

```
void MQTT_connect() {

  if (mqttClient.connected()) {
    return;
  }
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    while (1);
  }
  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
}
```

That's about all for now. If I enhance this further or build an actual case for it, I'll update this project.
