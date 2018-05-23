#include <BTLE.h>
#include <SPI.h>
#include <RF24.h>
#include <ESP8266WiFi.h>
#include "ArduinoJson.h"

#define CE D2 
#define CS D8
//Pins
//IRQ = D1
//MOSI = D7 = GPIO 13
//MISO = D6 = GPIO 12
//SCK = D5 = GPIO 14
//CE = D2 = GPIO 4
//CS = D8 = GPIO 15
//VCC = 3,3V!
//GND = G

//Bluetooth LE params
RF24 radio(CE, CS);
BTLE btle(&radio);

//Wifi params
char ssid[] = "313-ABEG";     //  your network SSID (name)
char pass[] = "ShitFuck69";  // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status
WiFiClient client;

//Http request params
char owmKey[] = "683b16583aaeb41eeb4af62c0956e971";
char owmRequest[] = "/data/2.5/weather?q=Budapest,hu&APPID=";
char owmServer[] = "api.openweathermap.org";

//Start services
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial) { }
  Serial.println("Device active");
  //Serial.end();

  // Set BTLE params and name - name: 8 chars max !
  btle.begin("wea");
  Serial.println("BTLE active");

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  pinMode(BUILTIN_LED, OUTPUT);

  //LED light
  // Connect D0 to RST to wake up
  pinMode(D0, WAKEUP_PULLUP);
  digitalWrite(BUILTIN_LED, LOW);
}

//Message types
const char MSG_TEMP   = 0;
const char MSG_HUMID  = 1;

//Get message from the result of query (with json-parsing)
// https://arduinojson.org/assistant/
String getTempFromJson(char* json, char msgType) {

  const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + 
                            JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 390;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root = jsonBuffer.parseObject(json);
  float temp = 0.0;
  float humidity = 0.0;
      switch(msgType) {
    case MSG_TEMP:
      temp = root["main"]["temp"];
      temp = temp - 273.15;
      return String(temp);  
    case MSG_HUMID:
      humidity = root["main"]["humidity"];
      return String(humidity);
    default:
      return String("");
  }
}

//Create Http-request and process result as a message-string
String getOwmTemp(char msgType) {

  if (client.connect(owmServer, 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    String req = String("GET ") + String(owmRequest) + String(owmKey) + String(" HTTP/1.1"); 
    String host = String("Host: ") + String(owmServer);
    Serial.println(host);
    client.println(req);
    client.println(host);
    client.println("Connection: close");
    client.println();
  } else {
    Serial.println("connection failed");
  }

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return String("");
    }
  }
  
  // Read all the lines of the reply from server and print them to Serial
  String json = "";
  while(client.available()){
    String line = client.readStringUntil('\n');
    if (line.charAt(0) == '{') {
      json += line;
    }
      
  }

  char jsonChar[json.length()+1];
  json.toCharArray(jsonChar, json.length()+1);
  //Get temp from json file  
  return getTempFromJson(jsonChar, msgType);
}

//Get the result of query and send it via BTLE
void sendMessage(char msgType) {
  
  String message = getOwmTemp(msgType);
  
  char buf[message.length()+1];
  message.toCharArray(buf, message.length()+1);

  for (int i = 0; i < 40; i++) {
    if(!btle.advertise(0x16, &buf, sizeof(buf))) {  //0x16
      Serial.println("BTLE advertisement failure");
      Serial.println(sizeof(buf));
    } else {
      Serial.print("BTLE advertisement successful: ");
      Serial.println(message);
    }
    delay(50);
  }
}

//Wait for a BTLE request from the phone, and send answer if needed
void loop() {
  //WAITING FOR BTLE REQUEST - max 21B payload!! - device name
  if (btle.listen()) {
  ///if (true) {
    
    //delay(50);
    uint8_t identify[] = {18, 33, 188, 190};  //HEX: 12 21 BC BE
    if (btle.buffer.pl_size < 4) return;
    if (btle.buffer.payload[0] != identify[0] || btle.buffer.payload[1] != identify[1] 
        || btle.buffer.payload[2] != identify[2] || btle.buffer.payload[3] != identify[3])
        return;
    
    Serial.print("Got payload: ");
    for (uint8_t i = 0; i < (btle.buffer.pl_size)-6; i++) {
      Serial.print(btle.buffer.payload[i],HEX);
      Serial.print(" "); 
    }
    
    char msgType = btle.buffer.payload[(btle.buffer.pl_size)-7];
    Serial.print("msgtype: ");
    Serial.println(msgType == MSG_TEMP ? "MSG_TEMP" : "MSG_HUMID");
    
    //ANSWER THE BTLE REQUEST
    sendMessage(msgType);   
  }

  //btle.hopChannel();
}

