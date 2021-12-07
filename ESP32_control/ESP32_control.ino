#include <Arduino.h>
#ifdef ESP32
#include "WiFi.h"
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <WiFiAP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include "WEB.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <string.h>
#include <time.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <math.h>
#include <stdio.h>
//function definitions1

//Function: notFound
//Description: if http not found return 404
//args:
//1. handler for requests.
void notFound(AsyncWebServerRequest *request);

//Function: callback
//description: callback for mqtt. if a message is received this function will be called.
void callback(char* topic, byte* message, unsigned int length);

//Function jsondata
//Description: convert readings into a json file.
void jsondata();

//Function: wifi_AP
//Description: function to enable the system to work as an accespoint. 
//ssid = "CTH_5"
//password = "Testing1234"
void wifi_AP();

//Function: wifi_setup
//description: sets up the wifi. The firsttime it will make an accesspoint. 
//then after configured it will try to connect to an existing network
//if it fails than accesspoint.
void wifi_setup();

//Function: server_setup
//Description: sets up callbacks for when a request is made.
//if te request is not defined it will go to function notFound.
void server_setup();

//Function printAddress
//description: function to print out addresses for the ds18b20.
void printAddress(DeviceAddress deviceAddress);

//Function sensor_setup
//Description: initializes the sensors. then check if ds18b20 is available.
void sensor_setup();

//Function: setup_eeprom
//Discription: init eeprom. checks if a config is already saved. if not save it. 
//then read it from the eeprom and copy the buffer into the config struct.
void setup_eeprom();

//Function: reconnect
//Description: connect to the mqtt server.
void reconnect();

//Function: printTemperature
//Description: print the temperature form the ds18b20. returns temperature as float.
float printTemperature(DeviceAddress deviceAddress);

//Function: getTime
//Description: gets time from a ntp server. returns the epoch time. print time the first time through the function.
unsigned long getTime();
//define debug for serial debugging
#define DEBUG
//#define DEBUG(format) printcheck(format)

#define HEATING 1
#define SPRAYING 2
#define DEHUMIFYING 3
#define IDLE_CASE 4

//pinout define and sensor libraries defines
#define DHTPIN2 5
#define DHTPIN3 4
#define DHTPIN4 18
#define DHTTYPE DHT11
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9 // Lower resolution
#define RELAY_FANIN 5
#define RELAY_FANOUT 6
#define RELAY_PUMP 7
#define RELAY_SPRINKLE 8
#define FANIN_CONTROL(bool) digitalWrite(RELAY_FANIN,bool);
#define FANOUT_CONTROL(bool) digitalWrite(RELAY_FANOUT,bool);
#define PUMP_CONTROL(bool) digitalWrite(RELAY_PUMP,bool);
#define SPRINKLE_CONTROL(bool) digitalWrite(RELAY_SPRINKLE,bool);

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64

#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht1(DHTPIN2, DHTTYPE);
DHT dht2(DHTPIN3, DHTTYPE);
DHT dht3(13, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
uint8_t numberOfDevices;
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

//eeprom handler
Preferences eeprom;

//allocated storage for json conversion
DynamicJsonDocument doc(400);
DynamicJsonDocument Senddoc(300);
DynamicJsonDocument wifi(300);
//variables for serialized json
char serializedjson[300];
bool isjson = 0;

bool control_temperature;
bool RDY = 1;
bool firsttime = 1;
bool mqtt_con = 1;

//webserver handler
AsyncWebServer server(80);
//MQTT handler
WiFiClient espClient;
PubSubClient client(espClient);

//variable for time management
double Absolute_humi;
long lastMsg;
unsigned long epochTime; 

//variable for storing the size of the webinterface files.
int const html_len = sizeof(html);
int const mystyle_len = sizeof(mystyle);
int const favicon_len = sizeof(favicon);

//struct for settings. if changed here you need to run eeprom.putBytes("configure",&c, sizeof(Config)); to save and execute.
struct Config {
  bool wifi_set = 1;
  char ssid[20] = "Janmaat";
  char password[20] = "Qvuv47zy4NTz";
  char mqtt_server[30] = "test.mosquitto.org";
  char ntpServer[30] = "nl.pool.ntp.org";
  int setpumpspeed= 255;
  float settemperature = 21;
  float setHumidity = 50;
  long readTime = 4000;
};

//struct for temperature data.
struct dht11_data {
  float temperature = 21.3;
  float humi = 33.2;
};

//struct for all sensordata combined.
struct sensordata {
  dht11_data DHT1;
  dht11_data DHT2;
  dht11_data DHT3;
  float DS18B20[2];
  uint8_t case_state = 4;
};

//declaring structs
struct sensordata registerdata;
struct Config c;

//symbol for oled display
unsigned char wifi1_icon16x16[] =
{
  0b00000000, 0b00000000, //                 
  0b00000111, 0b11100000, //      ######     
  0b00011111, 0b11111000, //    ##########   
  0b00111111, 0b11111100, //   ############  
  0b01110000, 0b00001110, //  ###        ### 
  0b01100111, 0b11100110, //  ##  ######  ## 
  0b00001111, 0b11110000, //     ########    
  0b00011000, 0b00011000, //    ##      ##   
  0b00000011, 0b11000000, //       ####      
  0b00000111, 0b11100000, //      ######     
  0b00000100, 0b00100000, //      #    #     
  0b00000001, 0b10000000, //        ##       
  0b00000001, 0b10000000, //        ##       
  0b00000000, 0b00000000, //                 
  0b00000000, 0b00000000, //                 
  0b00000000, 0b00000000, //                 
};

//Function: AH_calc
//Description: calculates the absolute humidity.
//args:
//float RH_inside: relative humidity
//float T_dry: temperature
//return:
//double x: absolute humidity.
double AH_calc(float RH_inside, float T_dry){
  RH_inside = RH_inside/100;
  float u = 56.8191;
  static const double E = 2.718281828459045;
  double h, t, s, x;
  
  t = RH_inside * 610.78 * exp(((17.08085 * (T_dry - 0)) / (234.175 + T_dry - 0)));;
  s = 101325 * pow(E,-(u/((T_dry + 273.15) * 8.314)));
  x = 622 * t/(s - t);
  #ifdef DEBUG
  Serial.printf("x = %f\n\r",x);
  #endif
  return x;
}
//Function: notFound
//Description: if http not found return 404
//args:
//1. handler for requests.
void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}
//Function: callback
//description: callback for mqtt. if a message is received this function will be called.
void callback(char* topic, byte* message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}
//Function jsondata
//Description: convert readings into a json file.
void jsondata()
{
  Senddoc["Temperature_GH"] = registerdata.DHT1.temperature;
  Senddoc["Humidity_GH"] = registerdata.DHT1.humi;
  Senddoc["Temperature_TH"] = registerdata.DHT2.temperature;
  Senddoc["Humidity_TH"] = registerdata.DHT2.humi;
  Senddoc["Temperature_FH"] = registerdata.DHT3.temperature;
  Senddoc["Humidity_FH"] = registerdata.DHT3.humi;
  Senddoc["DS18B20_TOP"] = registerdata.DS18B20[0];
  Senddoc["DS18B20_BOTTOM"] = registerdata.DS18B20[1];
  Senddoc["case_state"] = registerdata.case_state;
  Senddoc["mqtt_connected"] = mqtt_con;
} 
//Function: wifi_AP
//Description: function to enable the system to work as an accespoint. 
//ssid = "CTH_5"
//password = "Testing1234"
void wifi_AP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CTH_5", "Testing1234");
  delay(100);
  IPAddress Ip(192, 168, 178, 1); //sets ip of the device
  IPAddress NMask(255, 255, 255, 0); //mask
  WiFi.softAPConfig(Ip, Ip, NMask);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP); //prints ip adress of the device. ip is Ip variable.
}

//Function: wifi_setup
//description: sets up the wifi. The firsttime it will make an accesspoint. 
//then after configured it will try to connect to an existing network
//if it fails than accesspoint.
void wifi_setup()
{
  if(c.wifi_set == 1) //wifi_set will be set to 1 when a change was made using the webinterface.
  {                   //with the set ssid and pass the system will first try to connect to the wifi. if it fails it will start the accesspoint again.
    //sets up a wifi
    Serial.println(c.ssid);
    Serial.println(c.password);
    WiFi.mode(WIFI_STA);  //set mode to connect to an existing wifi.
    WiFi.begin(c.ssid, c.password);
    Serial.println("");
    // Wait for connection
    int i = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      i++;
      if(i == 40) break; //time out for trying to connect.
    }
    if (WiFi.status() == WL_CONNECTED) //if connected to an existing wifi it will set up mqtt to connect to the configured server.
    {
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(c.ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      client.setServer(c.mqtt_server, 1883); //connect to mqtt server
      client.setCallback(callback);          //set callback function
    }
    else //if not connected.
    {
      WiFi.disconnect(true); //reset wifi and start the accesspoint.
      wifi_AP();
      c.wifi_set = 0;
    }
  }
  else
  {
    wifi_AP();
  }
}
//Function: wifi_scan
//description: scans wifi networks nearby and stores them in a
//json file, ready to be send to the user interface.
void wifi_scan(){
  WiFi.disconnect(true);
  int n = WiFi.scanNetworks();
  char temp[10];
  if (n == 0) {
      Serial.println("no networks found");
  } else {
    for (int i = 0; i < n; ++i) {
      sprintf(temp, "wifi_%d", i);
      wifi[temp] = WiFi.SSID(i);
    }
  }
}
//Function: server_setup
//Description: sets up callbacks for when a request is made.
//if te request is not defined it will go to function notFound.
void server_setup()
{
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon , favicon_len);
    request->send(response);
  });

  server.on("/mystyle.css", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", mystyle , sizeof(mystyle));
    request->send(response);
  });

  //When request is commited to the ip address this file will be return. then the webinterface will send another request for the favicon and the mystyle files.
  //the webinterface also request the current config with api/config.
  //The webinterface will request api/Stats every 5000 ms. 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", html, sizeof(html));
    request->send(response);
  });

  //the webinterface will send a get request to the system every 5000ms. The system will reply with the sensordata.
  server.on("/api/Stats", HTTP_GET, [] (AsyncWebServerRequest * request)
  {
    jsondata();
    serializeJson(Senddoc, serializedjson); //serialises data to be send.
    request->send(200, "application/json", (String)serializedjson);
  });

  server.on("/api/wifi", HTTP_GET, [] (AsyncWebServerRequest * request)
  {
    serializeJson(wifi, serializedjson); //serialises data to be send.
    request->send(200, "application/json", (String)serializedjson);
  });
  
  //the webinterface will send a get request to the system to get the current config.
  server.on("/api/config", HTTP_GET, [] (AsyncWebServerRequest * request)
  {
    doc["Spump"] = c.setpumpspeed;
    doc["Stemp"] = c.settemperature;
    doc["Shumi"] = c.setHumidity;
    doc["Stime"] = c.readTime;
    doc["wifi_ssid"] = c.ssid;
    doc["wifi_pass"] = c.password;
    doc["mqtt_server"] = c.mqtt_server;
    
    serializeJson(doc, serializedjson);
    request->send(200, "application/json", (String)serializedjson);
  });
  
  //The webinterface will request a post. it will send his settings. The system will process the incoming configurations and then reboot.
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest * request)
  {
    String message;
    if (isjson == 1)
    {
      message = "json found!";
      isjson = 0;
    }
    else
    {
      message = "no json found!";
    }
    request->send(200, "text/plain", "Data received:" + message); //send a 200 reply that the data has been received.
  }, NULL, [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
  { //here the processing will happen. the data will first be deserialized and the put in to a struct.
    DeserializationError error;
    if (!index)
    {
      // Deserialize the JSON document
      error = deserializeJson(doc, (const char*)data);
    }
    // Test if parsing succeeds.
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else
    {
      isjson = 1;
      c.setpumpspeed = doc["Spump"];
      c.settemperature = doc["Stemp"];
      c.setHumidity = doc["Shumi"];
      c.readTime = doc["Stime"];
      
      size_t doc_len = sizeof(doc["wifi_ssid"]);
      const char* temp = doc["wifi_ssid"];
      memset(&c.ssid, 0, sizeof(c.ssid));
      strcpy(c.ssid, temp);
      
      doc_len = sizeof(doc["wifi_pass"]);
      temp = doc["wifi_pass"];
      memset(&c.password, 0, sizeof(c.password));
      strcpy(c.password, temp);

      doc_len = sizeof(doc["mqtt_server"]);
      temp = doc["mqtt_server"];
      memset(&c.mqtt_server, 0, sizeof(c.mqtt_server));
      strcpy(c.mqtt_server, temp);
      c.wifi_set = 1;
      
#ifdef DEBUG
      Serial.print("Pump Speed:");
      Serial.println(c.setpumpspeed);
      Serial.print("temperature:");
      Serial.println(c.settemperature);
      Serial.print("Humidity:");
      Serial.println(c.setHumidity);
      Serial.print("Read interval time:");
      Serial.println(c.readTime);
      Serial.print("wifi ssid:");
      Serial.println(c.ssid);
      Serial.print("wifi pass:");
      Serial.println(c.password);
      Serial.print("mqtt server:");
      Serial.println(c.mqtt_server);
#endif
      eeprom.putBytes("configure",&c, sizeof(c)); //when all settings are in the struct the system will reboot.
      delay(2000);
      ESP.restart();//reboot
    }
  });
  server.onNotFound(notFound);
}

//Function printAddress
//description: function to print out addresses for the ds18b20.
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
//Function sensor_setup
//Description: initializes the sensors. then check if ds18b20 is available.
void sensor_setup()
{
  dht1.begin();
  dht2.begin();
  dht3.begin();

  sensors.begin();
  numberOfDevices = sensors.getDeviceCount();
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();

      Serial.print("Setting resolution to ");
      Serial.println(TEMPERATURE_PRECISION, DEC);

      // set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
      sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);

      Serial.print("Resolution actually set to: ");
      Serial.print(sensors.getResolution(tempDeviceAddress), DEC);
      Serial.println();
    } 
    else
    {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
}

//Function: setup_eeprom
//Discription: init eeprom. checks if a config is already saved. if not save it. 
//then read it from the eeprom and copy the buffer into the config struct.
void setup_eeprom()
{
  eeprom.begin("config", false);
  //eeprom.putBytes("configure",&c, sizeof(Config));
  size_t schLen = eeprom.getBytesLength("configure");
  if(schLen == 0)
  {
    eeprom.putBytes("configure",&c, sizeof(Config));
  }
  char test[schLen];
  eeprom.getBytes("configure", test, schLen);
  if (schLen % sizeof(c))
  { // simple check that data fits
    Serial.println("Data is not correct size!");
  }
  memcpy(&c,&test,schLen);
  
}

//Function: reconnect
//Description: connect to the mqtt server.
void reconnect()
{
  int i = 0;
  // Loop until we're reconnected
  while (!client.connected())
  {
    // Attempt to connect
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX); //get random number for the client id
    if (client.connect(clientId.c_str()))
    { //start connecting
      Serial.println("MQTT connection establisted");
      // Subscribe
      client.subscribe("esp32/output"); //subscrip to topic if connected
      mqtt_con = 1;
    }
    else{
      if(i >= 5){
        mqtt_con = 0;
        break;
      }
      i++;
    }
  }
}
//Function: printTemperature
//Description: print the temperature form the ds18b20. returns temperature as float.
float printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C)
  {
    Serial.println("Error: Could not read temperature data");
    return tempC;
  }
  return tempC;
}

//Function: getTime
//Description: gets time from a ntp server. returns the epoch time. print time the first time through the function.
unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    #ifdef DEBUG
    Serial.println("Failed to obtain time");
    #endif
    return(0);
  }
  if(firsttime == 1)
  {
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  firsttime = 0;
  }
  time(&now);
  return now;
}


//function: choose_state
//desscription: uses AH_calc to calculate absolute humi. Then checks if measured temperature/humidity is within the tolerance.
//if not then choose the state that needs to be used and return the state.
//else go into idle.
//return:
//uint8_t number: returns a state. states are defined in uppersection of the code.
uint8_t choose_state()
{
  Absolute_humi = AH_calc(registerdata.DHT1.humi, registerdata.DHT1.temperature);
  double SetAbsolute_humi = AH_calc(c.setHumidity, registerdata.DHT1.temperature);
  if(registerdata.DHT1.temperature < c.settemperature && registerdata.DHT1.temperature < c.settemperature + 2)
  {
    return HEATING;  //verwarmen  
  }
  else if(registerdata.DHT1.temperature > c.settemperature  && registerdata.DHT1.temperature < c.settemperature - 2)
  {
    control_temperature = 1;
    return SPRAYING; //sproeien
  }
  else if(Absolute_humi < SetAbsolute_humi && Absolute_humi < AH_calc(c.setHumidity - 2, registerdata.DHT1.temperature))
  {
    control_temperature = 0;
    return SPRAYING; //sproeien
  }
  else if(Absolute_humi > SetAbsolute_humi && Absolute_humi > AH_calc(c.setHumidity + 2, registerdata.DHT1.temperature))
  {
    return DEHUMIFYING; //ontvocthigen
  }
  else 
    return IDLE_CASE; //go into idle mode
}

//function: turnoff
//desctiption: turns motors ands fans off
void turnoff()
{
  FANIN_CONTROL(0);
  FANOUT_CONTROL(0);
  PUMP_CONTROL(0);
  SPRINKLE_CONTROL(0);
}
//function: start_display
//description: initializes display and write text to screen.
void start_display()
{
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(20, 0);     // Start at top-left corner
  display.cp437(true);
  display.write("Capture");
  display.setCursor(15,20);
  display.write("The Heat");
  display.setCursor(45,40);
  display.write("5.0");
  display.display();
}
//function: topbar
//description: places wifi icon on the top off the screen.
void topbar(){
  display.drawBitmap(128-16,0, wifi1_icon16x16, 16,16,1);
  //if more symbols needs to be added
  //HERE!
}

//function: display1
//description: sets state number to text for display. Adds readings from sensors to the display.
void display1(char state){
  char temp[20];
  display.clearDisplay();
  if(c.wifi_set == 1) topbar();
  display.setCursor(0,16);
  display.setTextSize(1);
  switch(state)
  {
    case 1: display.write("Case: HEATING\n");break;
    case 2: display.write("Case: SPRAYING\n"); break;
    case 3: display.write("Case: DEHYMIDIFYING\n"); break;
    case 4: display.write("Case: IDLE\n");; break;
  }
  display.write("Greenhouse TEMP: ");
  sprintf(temp, "%.1f", registerdata.DHT1.temperature);
  display.write(temp);
  display.write("\nGreenhouse HUMI: ");
  sprintf(temp, "%.1f", registerdata.DHT1.humi);
  display.write(temp);
  display.write("\nGreenhouse AH: ");
  sprintf(temp, "%.4f", Absolute_humi);
  display.write(temp);
  display.display();
}

//function: printstate
//description: sets state number to text for serial interface.
void printstate(uint8_t state)
{
  switch(state)
  {
    case 1: Serial.println("HEATING"); break;
    case 2: Serial.println("SPRAYING"); break;
    case 3: Serial.println("DEHUMIFYING"); break;
    case 4: Serial.println("IDLE"); break;
  }
}

void setup()
{
  start_display();
  //sets serial
  Serial.begin(115200);
  setup_eeprom();
  sensor_setup();
  wifi_scan();
  wifi_setup();
  server_setup();
  server.begin();
  configTime(7200, 0, c.ntpServer);
  epochTime = getTime();
  Serial.print("epoch Time:");
  Serial.println(epochTime);
  display1(registerdata.case_state);
}

void loop()
{
  if(c.wifi_set == 1 && mqtt_con == 1) //if connected to an existing wifi than connect to a mqtt server.
  {
    if (!client.connected())
    {
      reconnect();
    }
    client.loop();
  }
  
  //
  //
  //set pump/read sensor/control fan !HERE!
  //
  //
  
  long now = millis();
  if (now - lastMsg > c.readTime)//interval time between readings. can be set via the webinterface.
  { 
    lastMsg = now;
    if(c.wifi_set == 1)
    {
      epochTime = getTime(); //gets current time if connected to wifi
      Senddoc["Timestamp"] = epochTime;
    }

    //reads the sensors and stores it in the sensordata struct
    registerdata.DHT1.temperature = dht1.readTemperature();
    registerdata.DHT1.humi = dht1.readHumidity();
    registerdata.DHT2.temperature = dht2.readTemperature();
    registerdata.DHT2.humi = dht2.readHumidity();
    registerdata.DHT3.temperature = dht3.readTemperature();
    registerdata.DHT3.humi = dht3.readHumidity();
    sensors.requestTemperatures(); // Send the command to get temperatures
    for (int i = 0; i < numberOfDevices; i++)
    {
      // Search the wire for address
      if (sensors.getAddress(tempDeviceAddress, i))
      {
        // Output the device ID
        registerdata.DS18B20[i] = printTemperature(tempDeviceAddress); // Use a simple function to print out the data
      }
      //else ghost device! Check your power requirements and cabling

    }
    if(c.wifi_set == 1 && mqtt_con == 1)
    { 
      //if connected to wifi then send a mqtt message
      jsondata();
      serializeJson(Senddoc, serializedjson);
      client.publish("esp32/data", serializedjson);
    }
    
    if(RDY == 1)
    {
      registerdata.case_state = choose_state();
      #ifdef DEBUG
      Serial.print("Current state: ");
      printstate(registerdata.case_state);
      turnoff();
      #endif
    }
    
    switch(registerdata.case_state)
    {
      case DEHUMIFYING:
        FANIN_CONTROL(1);
        FANOUT_CONTROL(1);
        PUMP_CONTROL(1);
        display1(DEHUMIFYING);
        if(registerdata.DHT1.humi <= c.setHumidity)
        {
          RDY = 1;
        }
        else
        {
          RDY = 0;
        }
        break;
      case HEATING:
        display1(HEATING);
        if(registerdata.DHT1.temperature >= c.settemperature)
        {
          RDY = 1;
        }
        else
        {
          RDY = 0;
        }
        break;
      case SPRAYING:
        SPRINKLE_CONTROL(1);
        display1(SPRAYING);
        if(control_temperature == 1)
        {
          if(registerdata.DHT1.temperature <= c.settemperature)
          {
            RDY = 1;
          }
          else
          {
            RDY = 0;
          }
        } 
        else
        {
          if(registerdata.DHT1.humi >= c.setHumidity)
          {
            RDY = 1;
          }
          else
          {
            RDY = 0;
          }
        }
        break;
      case IDLE_CASE:
        display1(IDLE_CASE);
        break;
      default:
        //never allowed!!
        break;
    }
  }
}
