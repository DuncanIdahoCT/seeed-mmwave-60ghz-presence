#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoMqttClient.h>
#include <WiFi.h>

#ifndef _RADAR_H__
#define _RADAR_H__

#define MESSAGE_HEAD 0x53       //Data frame header
#define MESSAGE_TAIL 0x54       //Data frame tail

#define HUMAN_PSE_RADAR 0x80    //Human presence data

#define PRESENCE_INF 0x01       //Presence Information
#define SOMEONE_HERE 0x01       //Someone here
#define NOONE_HERE 0x00         //Noone here

#define MOVE_INF 0x02           //Campaign Information
#define NONE 0x00               //None
#define STATIONARY 0x01         //A person is stationary
#define MOVEMENT 0x02           //A person in motion

#define BODY_SIG 0x03           //Body movement information

#define DISTANCE 0x04           //Distance from the person being detected

#define MOVESPEED 0x06          //Speed of character movement

#define FALL_DETECTION 0x83     //Fall data markers

#define FALL_STATE 0x01         //Fall status marker
#define NO_FALL 0x00            //No falls detected
#define FALLING 0x01            //Fall detected

#define FALL_POTENTIAL 0x02     //Confidence level for falls

#define FALL_LOCATION 0x03      //Location of the fall

#define POINTCLOUD_DATA 0x04    //Point cloud data

class FallDetection_60GHz{
    private:
        
    public:
        const byte MsgLen = 12;
        byte dataLen = 12;
        byte Msg[12];
        boolean newData = false;
        void SerialInit();
        void recvRadarBytes();
        void Fall_Detection(byte inf[]);
        void Situation_judgment(byte inf[]);
        void ShowData(byte inf[]);
};

#endif

// If you're not using an ESP32 with Arduino as I am for this project, you'll need to change the #ifdef or break out these definitions from inside the #ifdef
#ifdef ESP32
	#define RXD2 23
	#define TXD2 5
#endif

// WiFi parameters
#define WLAN_SSID       "your_ssid"
#define WLAN_PASS       "your_wifi_password"

// MQTT vars
const char broker[] = "xxx.xxx.xxx.xxx";
int        port     = 1883;
const char statustopic[]  = "homeassistant/sensor/seed-mmwave_status/state";
const char presencetopic[]  = "homeassistant/binary_sensor/seed-mmwave_presence/state";
const char movementtopic[]  = "homeassistant/binary_sensor/seed-mmwave_movement/state";

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

FallDetection_60GHz radar;

const long interval = 1000;
unsigned long previousMillis = 0;
int count = 0;

void setup() {
  Serial.begin(115200);
  Serial.println(F("USB Serial Ready"));
  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  delay(10);
  Serial.print(F("Connecting to "));
  Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());

  // I'm not sure if this is requied but I set a unique client ID while debugging MQTT authentication:
  mqttClient.setId("seed-mmwave");

  // Despite the Mosquito docs, I was not able to use my Home Assistant username and password here, the host tosses up some invalid key error.
  // However, there was one I used, that I can't even recall how it was generated, for another MQTT client solution which has a long key instead of an HA password
  mqttClient.setUsernamePassword("mqtt_username", "mqtt_password_key");
  MQTT_connect();
  // fire up the radar!
  radar.SerialInit();
}

void loop() {
  MQTT_connect();
  radar.recvRadarBytes();                       //Receive radar data and start processing
  if (radar.newData == true) {                  //The data is received and transferred to the new list dataMsg[]
    byte dataMsg[radar.dataLen+3] = {0x00};
    dataMsg[0] = 0x53;                         //Add the header frame as the first element of the array
    for (byte n = 0; n < radar.dataLen; n++)dataMsg[n+1] = radar.Msg[n];  //Frame-by-frame transfer
    dataMsg[radar.dataLen+1] = 0x54;
    dataMsg[radar.dataLen+2] = 0x43;
    radar.newData = false;                     //A complete set of data frames is saved
    
    //radar.ShowData(dataMsg);                 //This would show the entire HEX frame message from the radar module for debug purposes only

    radar.Situation_judgment(dataMsg);         //Use radar built-in algorithm to output human motion status
  }
}

// This void function establishes the Serial connection with the Seeed mmWave Radar module
// It's important to note that on an ESP32, you should use Serial2
// Serial is used for USB COM Port and Serial1 is somehow reserved but I forget why
// Just use Serial2 and move on with your life :) I stumbled on this one for quite some time...
// Trying to use SoftwareSerial and then HardwareSerial... it was all unnecessary, Serial2 was always there for the asking
void FallDetection_60GHz::SerialInit(){
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("mmWave Serial Txd is on pin: "+String(TXD2));
  Serial.println("mmWave Serial Rxd is on pin: "+String(RXD2));
  Serial.println("mmWave Serial Ready");
}

// Receive data and process
void FallDetection_60GHz::recvRadarBytes(){
  static boolean recvInProgress = false;
  static byte ndx = 0;
  byte startMarker = MESSAGE_HEAD;            //Header frame
  byte endMarker = MESSAGE_TAIL;
  byte rb; // Each frame received
  while (Serial2.available() > 0 && newData == false)
  {
    rb = Serial2.read();
    if (recvInProgress == true)
    {                     // Received header frame
      if (rb != endMarker){ // Length in range
        Msg[ndx] = rb;
        ndx++;
      }
      else{
        recvInProgress = false;
        dataLen = ndx;
        ndx = 0;
        newData = true;
      }
    }
    else if (rb == startMarker){ // Waiting for the first frame to arrive
        recvInProgress = true;
    }
  }
}

//Radar transmits data frames for display via serial port
void FallDetection_60GHz::ShowData(byte inf[]){
  for (byte n = 0; n < dataLen+3; n++) {
    Serial.print(inf[n], HEX);
    Serial.print(' ');
  }
    Serial.println();
}

// Judgment of occupied and unoccupied, approach and distance
// For simplicity, I've blended in the MQTT state topic messages below for the presence and motion states
// Including sending the latest state change message, of any case, to the status state topic
void FallDetection_60GHz::Situation_judgment(byte inf[]){

    String status;
    String presence;
    String movement;

    bool retained = true;
    int qos = 1;
    bool dup = false;

  switch(inf[2]){
    case HUMAN_PSE_RADAR:
      switch(inf[3]){
        case PRESENCE_INF:
          switch(inf[6]){
            case NOONE_HERE:
              ShowData(inf);
              status += "Radar detects no one.";
              Serial.println(status);
              Serial.println("----------------------------");

              presence = "off";
              mqttClient.beginMessage(presencetopic, presence.length(), retained, qos, dup);
              mqttClient.print(presence);
              mqttClient.endMessage();

              break;
            case SOMEONE_HERE:
              ShowData(inf);
              status += "Radar detects somebody.";
              Serial.println(status);
              Serial.println("----------------------------");

              presence = "on";
              mqttClient.beginMessage(presencetopic, presence.length(), retained, qos, dup);
              mqttClient.print(presence);
              mqttClient.endMessage();

              break;
          }
          break;
        case MOVE_INF:
          switch(inf[6]){
            case NONE:
              ShowData(inf);
              status += "Radar detects None.";
              Serial.println(status);
              Serial.println("----------------------------");
              break;
            case STATIONARY:
              ShowData(inf);
              status += "Radar detects somebody stationary.";
              Serial.println(status);
              Serial.println("----------------------------");

              movement = "off";
              mqttClient.beginMessage(movementtopic, movement.length(), retained, qos, dup);
              mqttClient.print(movement);
              mqttClient.endMessage();

              break;
            case MOVEMENT:
              ShowData(inf);
              status += "Radar detects somebody in motion.";
              Serial.println(status);
              Serial.println("----------------------------");

              movement = "on";
              mqttClient.beginMessage(movementtopic, movement.length(), retained, qos, dup);
              mqttClient.print(movement);
              mqttClient.endMessage();

              break;
          }
          break;
        case BODY_SIG:
          ShowData(inf);
          status += "The radar identifies the current motion feature value is: ";
          Serial.println(status);
          Serial.println(inf[6]);
          Serial.println("----------------------------");
          break;
        case DISTANCE:
          ShowData(inf);
          status += "The distance of the radar from the monitored person is: ";
          Serial.println(status);
          Serial.print(inf[6]);
          Serial.print(" ");
          Serial.print(inf[7]);
          Serial.println(" cm");
          Serial.println("----------------------------");
          break;
        case MOVESPEED:
          ShowData(inf);
          status += "The speed of movement of the monitored person is: ";
          Serial.println(status);
          Serial.print(inf[6]);
          Serial.print(" ");
          Serial.print(inf[7]);
          Serial.println(" cm/s");
          Serial.println("----------------------------");
          break;
      }
      break;
  }

    Serial.print("Sending latest message to sensor status state topic: ");
    Serial.println(statustopic);
    Serial.println(status);
    mqttClient.beginMessage(statustopic, status.length(), retained, qos, dup);
    mqttClient.print(status);
    mqttClient.endMessage();

    Serial.println();
}

//Fall detection data frame decoding
void FallDetection_60GHz::Fall_Detection(byte inf[]){
  switch(inf[2]){
    case FALL_DETECTION:
      switch(inf[3]){
        case FALL_STATE:
          switch(inf[6]){
            case NO_FALL:
              ShowData(inf);
              Serial.println("Radar detects that the current no fall.");
              Serial.println("----------------------------");
              break;
            case FALLING:
              ShowData(inf);
              Serial.println("Radar detects current someone falling.");
              Serial.println("----------------------------");
              break;
          }
          break;
        case FALL_POTENTIAL:
          ShowData(inf);
          Serial.print("The confidence level for the current radar detection of a fall is: ");
          Serial.println(inf[6]);
          Serial.println("----------------------------");
          break;
        case FALL_LOCATION:
          ShowData(inf);
          Serial.print("The fall position is: ");
          Serial.print("x: ");
          Serial.print(inf[6]);
          Serial.print(" ");
          Serial.print(inf[7]);
          Serial.print(" ");
          Serial.print("y: ");
          Serial.print(inf[8]);
          Serial.print(" ");
          Serial.println(inf[9]);
          Serial.println("----------------------------");
          break;
        case POINTCLOUD_DATA:
          ShowData(inf);
          Serial.print("The point cloud data are: ");
          Serial.print("x: ");
          Serial.print(inf[6]);
          Serial.print(" ");
          Serial.print(inf[7]);
          Serial.print(" ");
          Serial.print("y: ");
          Serial.print(inf[8]);
          Serial.print(" ");
          Serial.print(inf[9]);
          Serial.print(" ");
          Serial.print("z: ");
          Serial.print(inf[10]);
          Serial.print(" ");
          Serial.println(inf[11]);
          Serial.println("----------------------------");
          break;
      }
      break;

    // Even though this is the "Fall Detection" radar module, I'm not using, nor have I seen, any of these messages
    // Some things aren't yet implimented if you read the Seeed documentation but honestly I problably won't use this anyway
    // For this reason, I don't have any MQTT topic or state related code here
  
  }
}

// This function connects or reconnects the MQTT client, I found this solution better than doing an endless polling loop
// And I'm sure it's much easier on the CPU of the poor ESP32 I'm using :)
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
  
  // MQTT Discovery Testing, this will create a base device under MQTT Intergration with 1 sensor and 2 binary_sensors
  // I'm using the sensor state for all "unhandled" messages from the Seeed 60Ghz radar for now
  // The plan is to separate out the presence detection messages from the motion and send them to the two binary_sensors
  
  DynamicJsonDocument config(512);
  config["name"] = "Status";
  config["object_id"] = "seed-mmwave_status";
  config["unique_id"] = "your_unique_id_here... I used a GUID";
  config["state_topic"] = "homeassistant/sensor/seed-mmwave_status/state";
  JsonObject device  = config.createNestedObject("device");
  device["identifiers"] = "Custom";
  device["name"] = "Seeed mmWave";
  device["model"] = "CS101";
  device["manufacturer"] = "Custom";
  device["sw_version"] = "1.0";

  serializeJsonPretty(config, Serial);

  mqttClient.beginMessage("homeassistant/sensor/seed-mmwave_status/config", (unsigned long)measureJson(config));
  serializeJson(config, mqttClient);
  mqttClient.endMessage();

  DynamicJsonDocument config1(512);
  config1["device_class"] = "Occupancy";
  config1["name"] = "Presence";
  config1["object_id"] = "seed-mmwave_presence";
  config1["unique_id"] = "your_unique_id_here... I used a GUID";
  config1["state_topic"] = "homeassistant/binary_sensor/seed-mmwave_presence/state";
  config1["payload_on"] = "on";
  config1["payload_off"] = "off";
  JsonObject device1  = config1.createNestedObject("device");
  device1["identifiers"] = "Custom";
  device1["name"] = "Seeed mmWave";
  device1["model"] = "CS101";
  device1["manufacturer"] = "Custom";
  device1["sw_version"] = "1.0";

  serializeJsonPretty(config1, Serial);

  mqttClient.beginMessage("homeassistant/binary_sensor/seed-mmwave_presence/config", (unsigned long)measureJson(config1));
  serializeJson(config1, mqttClient);
  mqttClient.endMessage();

  DynamicJsonDocument config2(512);
  config2["device_class"] = "motion";
  config2["name"] = "Movement";
  config2["object_id"] = "seed-mmwave_movement";
  config2["unique_id"] = "your_unique_id_here... I used a GUID";
  config2["state_topic"] = "homeassistant/binary_sensor/seed-mmwave_movement/state";
  config2["payload_on"] = "on";
  config2["payload_off"] = "off";
  JsonObject device2  = config2.createNestedObject("device");
  device2["identifiers"] = "Custom";
  device2["name"] = "Seeed mmWave";
  device2["model"] = "CS101";
  device2["manufacturer"] = "Custom";
  device2["sw_version"] = "1.0";

  serializeJsonPretty(config2, Serial);

  mqttClient.beginMessage("homeassistant/binary_sensor/seed-mmwave_movement/config", (unsigned long)measureJson(config2));
  serializeJson(config2, mqttClient);
  mqttClient.endMessage();

  // I'm not entirely sure how the above triple json docs effect memory on the Arduino(ESP32 in my case) but this does work for now.
  // As is my MO with code I tinker with, this just has to be a far more repetitive way of doing things than is necessary... but I have other priorities!

  // This part below is meant to "initialize" the sensors in Home Assistant so that they aren't showing Unknown upon creation or update of the sketch to the MCU
  // However, it's not necessary and causes a minor issue where if the sensor is detecting human presence when you update the sketch or if the MQTT host is found to need reconnection...
  // The below code will reset the presence binary_sensor to Clear even though it's Detected and that won't change again until the state from the actual sensor does
  // A minor issue but I'll work out a better way at some point... or this code can just be removed as it's really not needed.
  String status;
  String presence;
  String movement;

  bool retained = true;
  int qos = 1;
  bool dup = false;

  presence = "off";
  mqttClient.beginMessage(presencetopic, presence.length(), retained, qos, dup);
  mqttClient.print(presence);
  mqttClient.endMessage();

  movement = "off";
  mqttClient.beginMessage(movementtopic, movement.length(), retained, qos, dup);
  mqttClient.print(movement);
  mqttClient.endMessage();

  status = "Radar detects no one.";
  mqttClient.beginMessage(statustopic, status.length(), retained, qos, dup);
  mqttClient.print(status);
  mqttClient.endMessage();

}
