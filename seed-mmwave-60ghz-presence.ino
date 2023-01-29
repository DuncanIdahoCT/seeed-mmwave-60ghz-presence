#include <Arduino.h>
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

#ifdef ESP32
	#define RXD2 23
	#define TXD2 5
#endif

// WiFi parameters
#define WLAN_SSID       "your_ssid"
#define WLAN_PASS       "your_password"

// MQTT vars
const char broker[] = "your_mqtt_host";
int        port     = 1883;
const char topic[]  = "mmwave/bedroom";

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

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("mmwave-bedroom-60ghz-presence");

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword("your_mqtt_username", "your_mqtt_password");
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
    
    //radar.ShowData(dataMsg);                 //Serial port prints a set of received data frames
    radar.Situation_judgment(dataMsg);         //Use radar built-in algorithm to output human motion status
  }
}

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
void FallDetection_60GHz::Situation_judgment(byte inf[]){

    String payload;

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
              payload += "Radar detects no one.";
              Serial.println(payload);
              Serial.println("----------------------------");
              break;
            case SOMEONE_HERE:
              ShowData(inf);
              payload += "Radar detects somebody.";
              Serial.println(payload);
              Serial.println("----------------------------");
              break;
          }
          break;
        case MOVE_INF:
          switch(inf[6]){
            case NONE:
              ShowData(inf);
              payload += "Radar detects None.";
              Serial.println(payload);
              Serial.println("----------------------------");
              break;
            case STATIONARY:
              ShowData(inf);
              payload += "Radar detects somebody stationary.";
              Serial.println(payload);
              Serial.println("----------------------------");
              break;
            case MOVEMENT:
              ShowData(inf);
              payload += "Radar detects somebody in motion.";
              Serial.println(payload);
              Serial.println("----------------------------");
              break;
          }
          break;
        case BODY_SIG:
          ShowData(inf);
          payload += "The radar identifies the current motion feature value is: ";
          Serial.println(payload);
          Serial.println(inf[6]);
          Serial.println("----------------------------");
          break;
        case DISTANCE:
          ShowData(inf);
          payload += "The distance of the radar from the monitored person is: ";
          Serial.println(payload);
          Serial.print(inf[6]);
          Serial.print(" ");
          Serial.print(inf[7]);
          Serial.println(" cm");
          Serial.println("----------------------------");
          break;
        case MOVESPEED:
          ShowData(inf);
          payload += "The speed of movement of the monitored person is: ";
          Serial.println(payload);
          Serial.print(inf[6]);
          Serial.print(" ");
          Serial.print(inf[7]);
          Serial.println(" cm/s");
          Serial.println("----------------------------");
          break;
      }
      break;
  }

    Serial.print("Sending message to topic: ");
    Serial.println(topic);
    Serial.println(payload);
    mqttClient.beginMessage(topic, payload.length(), retained, qos, dup);
    mqttClient.print(payload);
    mqttClient.endMessage();

    Serial.println();

}

//Respiratory sleep data frame decoding
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
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
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
