#include "DHT.h"
#include "CRC.h"
#include "CRC16.h"
#include <HardwareSerial.h>
#define DHTPIN1 4
#define DHTPIN2 9
#define DHTPIN3 10
#define DEBUG
#define DHTTYPE DHT22
CRC16 crc;
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);
//HardwareSerial MySerial(1);
struct dht22_data {
  float temperature = 16.3;
  float humidity = 50.2;
};

struct sensordata {
  dht22_data dht[3];
};

union convert {
  float f;
  unsigned char b[4];
};

struct sensordata registerdata;
//startbyte, destination, function, length ,data, crc
byte data_buffer[50];
byte data_buffer_in[10];
int data_length;
long lastMsg;
uint16_t crccheck;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  //MySerial.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("Made by Quest 5 capture the heat");
  Serial.println("device reads sensors and sends it to the controller\n\n\n\n\r");
  dht1.begin();
  dht2.begin();
  dht3.begin();
  Serial.println("DHT22 sensors initialized");
}
void read_sensor() {
    registerdata.dht[0].humidity = dht1.readHumidity();
    registerdata.dht[0].temperature = dht1.readTemperature();
    registerdata.dht[1].humidity = dht2.readHumidity();
    registerdata.dht[1].temperature = dht2.readTemperature();
    registerdata.dht[2].humidity = dht3.readHumidity();
    registerdata.dht[2].temperature = dht3.readTemperature();
}

void PrintHex8(uint8_t *data, uint8_t length) // prints 8-bit data in hex with leading zeroes
{
  char tmp[16];
  for (int i = 0; i < length; i++) {
    sprintf(tmp, "%.2X", data[i]);
    Serial.print(tmp); Serial.print("");
  }
}

void data_req()
{
  int i, k;
  data_buffer[0] = 0x01;
  data_buffer[1] = 0x11;
  data_buffer[2] = 0x01;
  data_buffer[3] = 24;
  union convert c;
  for (i = 0; i < 3; i++) {
    c.f = registerdata.dht[i].humidity;
    for (k = 0; k < sizeof(c.b); k++) {
      data_buffer[(4 * i) + k + 4] = c.b[k];
    }
  }
  for (i = 0; i < 3; i++) {
    c.f = registerdata.dht[i].temperature;
    for (k = 0; k < sizeof(c.b); k++) {
      data_buffer[(4 * i) + k + 16] = c.b[k];
    }
  }
  crc.reset();
  for (i = 0; i < 24; i++) {
    crc.add(data_buffer[i + 4]);
  }
  crccheck = crc16((uint8_t *) data_buffer, 28, 0x1021, 0, 0, false, false);
  data_buffer[28] = crccheck >> 8;
  data_buffer[29] = crccheck;
  PrintHex8(data_buffer, 30);
  Serial.println("");
  for(i = 0; i<30; i++){
    //MySerial.write(data_buffer[i]);
  }
}
bool check_crc(){
  uint16_t temp;
  crc.reset();
  crccheck = crc16((uint8_t *) data_buffer_in, (data_length - 1), 0x1021, 0, 0, false, false);
  temp = data_buffer_in[data_length] | data_buffer_in[data_length - 1] << 8;
  if(crccheck == temp){
  Serial.println("crc succes");
  return 1;
  }
  else{
  Serial.println("crc failed");
  return 0;
  }
}
void handle_data() {
  while (true) {
    if(check_crc()){
      Serial.println("Data received");
      Serial.print("Adressed to: ");
      Serial.print(data_buffer_in[1],HEX);
      if (data_buffer_in[1] == 0x11)
      {
        Serial.println("me!");
        Serial.print("Function:    ");
        switch(data_buffer_in[2]){
          case 0x01:
            Serial.println("Data request");
            Serial.print("return:      ");
            data_req();
            break;
          case 0x02:
            Serial.println("reset");
            break;
          default:
            Serial.println("Unkown function");
            break;
        }
        break;
      }
      else
      {
        Serial.println("Not me!");
        break;
      }
    }
    break;
  }
}

bool incoming_data() {
  int i = 0;
  int k = 0;
  int incoming_byte = 0;
  bool new_data= false;
  
//  while (MySerial.available()) {
//    incoming_byte = MySerial.read();
//    if (incoming_byte == '\n') {
//      break;
//    }
//    data_buffer_in[i] = incoming_byte;
//    i++;
//    new_data = true;
//    delay(10);
//  }
  delay(100);
  if(data_buffer_in[0] == 0x01 && new_data == true){
    data_length = i - 1;
    new_data = false;
    handle_data();
  }
}
void loop()
{
  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    read_sensor();
    Serial.println("Get Temperature and Humidity");
#ifdef DEBUG
    Serial.println("sensor 1:");
    Serial.print(F("Humidity: "));
    Serial.print(registerdata.dht[0].humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(registerdata.dht[0].temperature);
    Serial.println();
    Serial.println("sensor 2:");
    Serial.print(F("Humidity: "));
    Serial.print(registerdata.dht[1].humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(registerdata.dht[1].temperature);
    Serial.println();
    Serial.println("sensor 3:");
    Serial.print(F("Humidity: "));
    Serial.print(registerdata.dht[2].humidity);
    Serial.print(F("%  Temperature: "));
    Serial.print(registerdata.dht[2].temperature);
    Serial.println();
#endif
  }
  bool datain;
  datain = incoming_data();
}
