 #include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define DHTTYPE DHT11

#define DHT11_SP A3
#define DHT11_LNO A1
#define DHT11_LNK A2

#define SMT_AN_SDA A4
#define SMT_AN_SCL A5
#define ONE_WIRE_BUS 12

#define pomp_pin_1 7
#define pomp_pin_2 4

#define ventilator_pin_1 5
#define ventilator_pin_2 6



DHT dht3(DHT11_SP, DHTTYPE);
DHT dht2(DHT11_LNO, DHTTYPE);
DHT dht1(DHT11_LNK, DHTTYPE);


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


void PT100_Sensoren();
int ventilator_control(int ventilator_1, int ventilator_2);
int pomp_control(int pomp_1, int pomp_2);
void DHT11_Sensoren();


void setup() {
  
  Serial.begin(9600);

  dht1.begin();
  dht2.begin();
  dht3.begin();
  sensors.begin();

  pomp_control(0,0);
  ventilator_control(0,0);
}

void loop() {

  DHT11_Sensoren();
  PT100_Sensoren();
  pomp_control(1,1);
  ventilator_control(255,255);
  //delay(500);
  //Serial.print("\r\n");
  
  Serial.println("");
  delay(100);
}


void PT100_Sensoren()
{
  sensors.requestTemperatures();
  float Temp1 = sensors.getTempCByIndex(0);
  float Temp2 = sensors.getTempCByIndex(1);


   Serial.print("tp100.b/o");
   Serial.print(",");

  //temperatuur sensor boven
  Serial.print(Temp1);
  Serial.print(",");

  //temperatuur sensor onder
  Serial.print(Temp2);
  Serial.print(",");
}



int ventilator_control(int ventilator_1, int ventilator_2)
{

  //controlle of de waarde niet buiten de range vallen.
  if(ventilator_1 > 1 || ventilator_1 < 0 || ventilator_2 > 1 || ventilator_2 < 0)
  {
    return 0;
  }
  
  //hier worden de pinnen van de pomp hoog of laag gezet.
  analogWrite(ventilator_pin_1, ventilator_1);
  analogWrite(ventilator_pin_2, ventilator_2);

  return 1;
}

int pomp_control(int pomp_1, int pomp_2)
{
  //controlle of de waarde niet buiten de range vallen.
  if(pomp_1 > 1 || pomp_1 < 0 || pomp_2 > 1 || pomp_2 < 0)
  {
    return 0;
  }
  
  //hier worden de pinnen van de pomp hoog of laag gezet.
  digitalWrite(pomp_pin_1, pomp_1);
  digitalWrite(pomp_pin_2, pomp_2);

  return 1;
}

void DHT11_Sensoren()
{

  float h1 = dht1.readHumidity();
  float t1 = dht1.readTemperature();

  float h2 = dht2.readHumidity();
  float t2 = dht2.readTemperature();

  float h3 = dht3.readHumidity();
  float t3 = dht3.readTemperature();

  Serial.print("kas.u/i/c");
  Serial.print(",");

  if (isnan(h1) || isnan(t1)) 
  {
    Serial.print("Failed");
    Serial.print(",");
    Serial.print("Failed");
    Serial.print(",");
  }
  else
  {

  Serial.print(t1);
  Serial.print(",");
  Serial.print(h1); 
  Serial.print(",");
 }

  if (isnan(h2) || isnan(t2)) 
  {
    Serial.print("Failed");
    Serial.print(",");
    Serial.print("Failed");
    Serial.print(",");
  }
  else
  {

  Serial.print(t2);
  Serial.print(",");
  Serial.print(h2);
  Serial.print(",");
  }


  
  if (isnan(h3) || isnan(t3)) 
  {
    Serial.print("Failed");
    Serial.print(",");
    Serial.print("Failed");
    Serial.print(",");
  }
  else
  {

  Serial.print(t3);
  Serial.print(",");
  Serial.print(h3);
  Serial.print(",");
  }
  
}
