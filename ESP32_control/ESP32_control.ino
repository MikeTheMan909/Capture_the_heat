#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
//#define OVERWIFI
#ifdef OVERWIFI

#else

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
void MOTOR_CONTROL(bool state);

#define DEBUG

//pinout define and sensor libraries defines
#define DHTPIN2 2
#define DHTPIN3 3
#define DHTPIN4 4
#define DHTTYPE DHT22
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 9 // Lower resolution
#define RELAY_FAN 5
#define MOTOR_EN_PIN 16
#define MOTOR_IN1_PIN 7
#define MOTOR_IN2_PIN 8
#define motorchannel 0
#define FAN_CONTROL(bool) digitalWrite(RELAY_FAN,bool);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);
DHT dht4(DHTPIN4, DHTTYPE);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Preferences prefs;

int numberOfDevices;
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

//allocated storage for json conversion
DynamicJsonDocument doc(200);
DynamicJsonDocument Senddoc(300);

char serializedjson[300];
bool isjson = 0;

//webserver handler
AsyncWebServer server(80);
//MQTT handler
WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg;
long readTime = 5000;
unsigned long epochTime; 

int const html_len = sizeof(html);
int const mystyle_len = sizeof(mystyle);
int const favicon_len = sizeof(favicon);
//password and ssid
const char *ssid = "CTH_5";
const char *password = "Testing1234";

struct Config {
  const char *ssid = "Janmaat";
  const char *password = "Qvuv47zy4NTz";
  const char* mqtt_server = "test.mosquitto.org";
  const char* ntpServer = "nl.pool.ntp.org";
  int setpumpspeed= 255;
  float settemperature;
  float setHumidity;
};

struct dht11_data {
  float temperature = 21.3;
  float humi = 33.2;
};

struct sensordata {
  dht11_data DHT1;
  dht11_data DHT2;
  dht11_data DHT3;
  float DS18B20[2];
};

struct sensordata registerdata;
struct Config c;

//if http not found return 404
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

//convert readings into a json file
void jsondata() {
  Senddoc["Temperature_GH"] = registerdata.DHT1.temperature;
  Senddoc["Humidity_GH"] = registerdata.DHT1.humi;
  Senddoc["Temperature_TH"] = registerdata.DHT2.temperature;
  Senddoc["Humidity_TH"] = registerdata.DHT2.humi;
  Senddoc["Temperature_FH"] = registerdata.DHT3.temperature;
  Senddoc["Humidity_FH"] = registerdata.DHT3.humi;
  Senddoc["DS18B20_TOP"] = registerdata.DS18B20[0];
  Senddoc["DS18B20_BOTTOM"] = registerdata.DS18B20[1];
}

void wifi_setup()
{
  //sets up a wifi
#ifdef OVERWIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(c.ssid, c.password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(c.ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  client.setServer(c.mqtt_server, 1883);
  client.setCallback(callback);
#else
  WiFi.softAP(c.ssid, c.password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
#endif
}

void server_setup() {
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon , favicon_len);
    request->send(response);
  });

  server.on("/mystyle.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", mystyle , sizeof(mystyle));
    request->send(response);
  });

  //is a GET request answered with some html things
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", html, sizeof(html));
    request->send(response);
  });

  //send a get request for reading the readings in json format
  server.on("/api/Stats", HTTP_GET, [] (AsyncWebServerRequest * request) {
    jsondata();
    serializeJson(Senddoc, serializedjson);
    request->send(200, "application/json", (String)serializedjson);
  });

  //send json to change temperature.
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest * request) {
    String message;
    if (isjson == 1) {
      message = "json found!";
      isjson = 0;
    } else {
      message = "no json found!";
    }
    request->send(200, "text/plain", "Data received:" + message);
  }, NULL, [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total)
  {
    DeserializationError error;
    if (!index)
      // Deserialize the JSON document
      error = deserializeJson(doc, (const char*)data);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      isjson = 1;
      c.setpumpspeed = doc["Spump"];
      c.settemperature = doc["Stemp"];
      c.setHumidity = doc["Shumi"];
#ifdef DEBUG
      Serial.print("Pump Speed:");
      Serial.println(c.setpumpspeed);
      Serial.print("temperature:");
      Serial.println(c.settemperature);
      Serial.print("Humidity:");
      Serial.println(c.setHumidity);
#endif
    }
  });
  server.onNotFound(notFound);
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void sensor_setup() {
  dht2.begin();
  dht3.begin();
  dht4.begin();

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
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
}
void motor_setup(){
  sigmaDeltaSetup(motorchannel, 312500);
  sigmaDeltaAttachPin(18,motorchannel);
  };
void setup() {
  //sets serial
  Serial.begin(115200);
//  prefs.begin("data");
//  prefs.getBytes("data",)
//  if(p)
  sensor_setup();
  motor_setup();
  wifi_setup();
  server_setup();
  server.begin();
  configTime(7200, 0, c.ntpServer);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
  }
}

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

void MOTOR_CONTROL(bool state)
{
  if (state == true)
  {
    sigmaDeltaWrite(motorchannel, c.setpumpspeed);
    ledcWrite(motorchannel, c.setpumpspeed);
    digitalWrite(MOTOR_IN1_PIN, HIGH);
    digitalWrite(MOTOR_IN2_PIN, LOW);
  }
  else
  {
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
  }
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  time(&now);
  return now;
}

void loop() {
  //logic with temperature and humidity
  //set pump/read sensor/control fans
#ifdef OVERWIFI
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
#endif
  if (false) {
    FAN_CONTROL(1);
    MOTOR_CONTROL(1);
  }
  long now = millis();
  if (now - lastMsg > readTime) {
    lastMsg = now;
    epochTime = getTime();
    Serial.print("epoch Time:");
    Serial.println(epochTime);
    registerdata.DHT1.temperature = dht2.readTemperature();
    registerdata.DHT1.humi = dht2.readHumidity();
    registerdata.DHT2.temperature = dht3.readTemperature();
    registerdata.DHT2.humi = dht3.readHumidity();
    registerdata.DHT3.temperature = dht4.readTemperature();
    registerdata.DHT3.humi = dht4.readHumidity();

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
#ifdef OVERWIFI
    Senddoc["Timestamp"] = epochTime;
    jsondata();
    serializeJson(Senddoc, serializedjson);
    client.publish("esp32/data", serializedjson);
#endif
  }
}
