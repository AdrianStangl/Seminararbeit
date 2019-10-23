#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Ticker.h>


#define USE_MQTT 1                //To use MQTT, install Library "PubSubClient" and switch next line to 1
#define USE_LOCAL_BUTTON 1        //If you don`t want to use local button switch to 0


#if USE_MQTT == 1
  #include <PubSubClient.h>
  //Your MQTT Broker
  const char* mqtt_server = "172.22.131.186";   //raspi IP
  const char* mqtt_in_topic = "set2";           //subscribed topic; receives messages        
  const char* mqtt_out_topic = "status2";       //publish topic; channel on which the status gets published
#endif


//Wifi data
const char* ssid = "hyperon";          //Yout Wifi SSID      
const char* password = "%354pHySiK%";    //Your Wifi Key

#if USE_MQTT == 1
  WiFiClient espClient;
  PubSubClient client(espClient);
  bool status_mqtt = 1;
  const int timer = 10000;  //timer off 10 sec, used for publishing the status in the loop
  unsigned long startTime;
  char currentState[10]; //variable for the current state of the socket, gets published, by the timer or message == "2"
#endif

//lights
int gpio13Led = 13; //green?
int gpio12Relay = 12; //blue?

bool lamp = 0; 
bool relais = 0; //1 == on ; 0 == off

//Test  GPIO 0 every 0.1 sec
Ticker checker;
bool status = 1;


void setup(void){
  Serial.begin(115200); 
  delay(5000);

  //init timer
  startTime = millis();
 
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  pinMode(gpio13Led, OUTPUT);
  digitalWrite(gpio13Led, lamp);
  
  pinMode(gpio12Relay, OUTPUT);
  digitalWrite(gpio12Relay, relais);

  // connect to ssid, blink while not connected
  Serial.print("Verbindungsaufbau mit:  ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(lamp == 0){
       digitalWrite(gpio13Led, 1);
       lamp = 1;
     }else{
       digitalWrite(gpio13Led, 0);
       lamp = 0;
     }
    Serial.print(".");
  }
  Serial.println("Verbunden");
  //if connected stop blinking and turn lamp off
  digitalWrite(gpio13Led, HIGH); 

//set the server with entered IP at port 1883
//set callback method for receiving published message to mqttCallback()
  #if USE_MQTT == 1  
    client.setServer(mqtt_server, 1883);
    client.setCallback(MqttCallback);
  #endif

  //Timer for checking the button every 0.1 sec
  checker.attach(0.1, check);
  
}


#if USE_MQTT == 1
  void MqttCallback(char* topic, byte* payload, unsigned int length) {
      if ((char)payload[0] == '1') { //if first char == 1 turn on
       Switch_On();
      } else if((char)payload[0] == '0'){      //first char == 0 switch off
       Switch_Off();
      } else if((char)payload[0] == '2'){      //first char == 2 pulish status  "on" or "off"
        client.publish(mqtt_out_topic, currentState, true);
      } else if((char)payload[0] == '3'){      //first char == 3 print IP, Gateway, subnetmask, macaddress
        client.publish(mqtt_out_topic, "IP: ");
        client.publish(mqtt_out_topic, WiFi.localIP().toString().c_str());
        client.publish(mqtt_out_topic, "Gateway: ");
        client.publish(mqtt_out_topic, WiFi.gatewayIP().toString().c_str());
        client.publish(mqtt_out_topic, "Netmask: ");
        client.publish(mqtt_out_topic, WiFi.subnetMask().toString().c_str());
        client.publish(mqtt_out_topic, "MAC-Address: ");
        client.publish(mqtt_out_topic, WiFi.macAddress().c_str());
      }
  }

void MqttReconnect() {
  String clientID = "SonoffSocket_"; // 13 chars
  clientID += WiFi.macAddress();//17 chars

  while (!client.connected()) {
    Serial.print("Connect to MQTT-Broker");
    if (client.connect(clientID.c_str(), NULL, NULL, mqtt_out_topic, 0, 0, "disconnected")) {
      Serial.print("connected as clientID:");
      Serial.println(clientID);
      //publish ready
      client.publish(mqtt_out_topic, "mqtt client ready");
      //subscribe to topic
      client.subscribe(mqtt_in_topic);
    } else {
      Serial.print("failed: ");
      Serial.print(client.state());
      Serial.println(" try again...");
      delay(5000);
    }
  }
}

//method for publishing the status;
//publishs either "on" or "off"
void MqttStatePublish() {
  if (relais == 1 and not status_mqtt)
     {
      status_mqtt = relais;
      client.publish(mqtt_out_topic, "on", true);
      snprintf (currentState, 10, "on");
      Serial.println("MQTT publish: on");
     }
  if (relais == 0 and status_mqtt)
     {
      status_mqtt = relais;
      client.publish(mqtt_out_topic, "off", true);
      snprintf (currentState, 10, "off");
      Serial.println("MQTT publish: off");
     }
}

#endif

//Methods for switching the socket on or off
//both publish there state if called
void Switch_On(void) {
  relais = 1;
  digitalWrite(gpio13Led, LOW);
  digitalWrite(gpio12Relay, relais);
  MqttStatePublish();
}
void Switch_Off(void) {
  relais = 0;
  digitalWrite(gpio13Led, HIGH);
  digitalWrite(gpio12Relay, relais);
  MqttStatePublish();
}

void check(void)
{
  
#if USE_LOCAL_BUTTON == 1
  //check gpio0 (button of Sonoff device)
  
  if ((digitalRead(0) == 0) and not status)
  {
    status=(digitalRead(0) == 0);
    if (relais == 0) {
      Switch_On();
  //Switch off
    } else {
      Switch_Off();
    }
  }
  status=(digitalRead(0) == 0);
#endif
}

void loop(void){

#if USE_MQTT == 1
//MQTT
   if (!client.connected()) { //client not connected
    MqttReconnect();
   }
   if (client.connected()) {  //client connected
    MqttStatePublish();
   }

//timer for publishing the status 
/*  if(millis() > startTime + timer){
    client.publish(mqtt_out_topic, currentState, true);
//    MqttStatePublish();
    startTime = millis();
  }
*/
  client.loop();
#endif  
} 
