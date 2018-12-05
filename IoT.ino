// Assume walking in will pass sensor1 first
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include "DHT.h"

#define sensor1 D1
#define sensor2 D2
#define bulb D0
#define temp_sensor D3
#define DHTTYPE DHT22
#define LINE_TOKEN "FxjwlCL1tYu1yLkO1BMyCqxHWRQZNnyo4wmfAG7Brtj"

DHT dht(temp_sensor, DHTTYPE);

//setup WiFi
const char* ssid= "Vinter";
const char* password = "goffblin";

ESP8266WiFiMulti WiFiMulti;
WiFiClient esp8266Client;
PubSubClient client(esp8266Client);

const char* mqtt_server = "35.239.160.127";
#define MQTT_PUB_TOPIC "personCount"
#define MQTT_ID "ESP32424242"

int sen1 = 0;//no detection found for sensor 1
int sen2 = 0; //no detection found for sensor 2
int prev1;
int prev2;
//detect if it's a walk-in or walk-out
bool in = false; 
bool out = false;
int count = 0; //number of people
//check if walk back when walk in or walk out
bool in_walkback = false; 
bool out_walkback = false;
bool pass1 = false; //pass sensor1
bool pass2 = false;
int loopcount = 0;
int durationCheck = 0;
String room = "101";

void setup_wifi() {
  // put your setup code here, to run once:

    delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFiMulti.addAP(ssid, password);
  delay(10);

  while (WiFiMulti.run() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_ID))
    {
      Serial.println("connected");
      // ... and resubscribe
      //client.subscribe(MQTT_SUB_TOPIC);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

void Line_Notify(String message) {
  WiFiClientSecure client;

  if (!client.connect("notify-api.line.me", 443)) {
    Serial.println("connection failed");
    return;   
  }

  String req = "";
  req += "POST /api/notify HTTP/1.1\r\n";
  req += "Host: notify-api.line.me\r\n";
  req += "Authorization: Bearer " + String(LINE_TOKEN) + "\r\n";
  req += "Cache-Control: no-cache\r\n";
  req += "User-Agent: ESP8266\r\n";
  req += "Content-Type: application/x-www-form-urlencoded\r\n";
  req += "Content-Length: " + String(String("message=" + message).length()) + "\r\n";
  req += "\r\n";
  req += "message=" + message;
  // Serial.println(req);
  client.print(req);
    
  delay(20);

  // Serial.println("-------------");
  while(client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
    //Serial.printl
  // Serial.println("-------------");
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  dht.begin();
  
  pinMode(sensor1, INPUT);
  pinMode(sensor2, INPUT);
  pinMode(bulb, OUTPUT);
  pinMode(temp_sensor, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  loopcount++;
  prev1 = sen1;
  prev2 = sen2;
  sen1 = digitalRead(sensor1);
  sen2 = digitalRead(sensor2);

  float t = dht.readTemperature();

  //For sensors, 0 = detect sth, 1 = nothing through

  //detect whether walking in or out
  if(sen1 == 0){
    if(!out){
      in = true;
      durationCheck = 0;
    }
  }
  if(sen2 == 0){
    if(!in){
      out = true;
      durationCheck = 0;
    }
  }

  //add/subtract count
  //check if pass from sen1 to sen2
  if(in && sen2 == 0 && sen1 == 1 && prev1 == 0){
    pass1 = true;
  }
  //after walking pass sen2
  if(in && sen1 == 1 && sen2 == 1 && prev2 == 0){
    count++;
    in = false;
    if(in_walkback){
      in_walkback = false;
    }
    Serial.println("in_pass2");
  }
  //in case walking back to sen1 to go out without going in
  if(in && sen1 == 0 && sen2 == 1 && prev2 == 0){
    in_walkback = true;
    Serial.println("in_walkback");
  }
  //reset in_walkback
  if(in_walkback && sen1 == 1 && prev1 == 0 && sen2 != 0){
    in_walkback = false;
    in = false;
  }
  
  //go out
  if(out && sen1 == 0 && sen2 == 1 && prev2 == 0){
     pass2 = true;
  }
  //after walking pass sen1
  if(out && sen2 == 1 && sen1 == 1 && prev1 == 0){
    count--;
    out = false;
    if(out_walkback){
      out_walkback = false;
    }
    Serial.println("out_pass2");
  }
  //in case walking back to sen2 to go in without going out
  if(out && sen2 == 0 && sen1 == 1 && prev1 == 0){
    out_walkback = true;
    Serial.println("out_walkback");
  }
  //reset out_walkback
  if(out_walkback && sen2 == 1 && prev2 == 0 && sen1 != 0){
    out_walkback = false;
    out = false;
  }

  if(count > 0){
    digitalWrite(bulb, HIGH);
  }else{
    digitalWrite(bulb, LOW);
  }

  String temp = "{\"room\":\"101\",\"floor\":1,\"count\":"+(String)count+",\"temp\":"+(String)t+"}";
  char buff[temp.length()+1];
  temp.toCharArray(buff,sizeof(buff));

  if(loopcount % 200 == 0){
    loopcount = 0;
    client.publish(MQTT_PUB_TOPIC,buff);
    if(t > 45){
      String message = "WARNING ! HIGH TEMPERATURE IN ROOM " + room;
      Line_Notify(message);
    }
    Serial.println(count);
    Serial.println(sen1);
    Serial.println(sen2);
    Serial.println(in);
    Serial.println(out);
    Serial.print("in_walkback ");
    Serial.println(in_walkback);
    Serial.print("out_walkback ");
    Serial.println(out_walkback);
    Serial.print("temp:");
    Serial.println(t);
  }

  if(durationCheck >= 300){
    if(!(sen1 == 0 || sen2 == 0)){
      durationCheck = 0;
      in = false;
      out = false;
    }
  }

  durationCheck++;
  
  if (!client.connected()) {
    reconnect();
  }
  
  delay(5);
}
