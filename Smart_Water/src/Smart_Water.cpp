/* 
 * Project Smart Plant Watering System
 * Author: Jon Phillips
 * Date: Last updated 8 Nov 2023
 * 
 *
 */

#include "Particle.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <Grove_Air_quality_Sensor.h>
#include <button.h>
#include <credentials.h>
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
SYSTEM_MODE(SEMI_AUTOMATIC);

int OLED_RESET(-1);
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_BME280 bme;
AirQualitySensor aqSensor(A1);
TCPClient TheClient; 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 
Adafruit_MQTT_Publish pubAQ = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Air Quality");
Adafruit_MQTT_Publish pubTemp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Temp");
Adafruit_MQTT_Publish pubMoist = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Soil Moisture");
Adafruit_MQTT_Publish pubHumid = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Humidity");
const int soilSensor = A5 ;
const int dustSensor = A0;
const int pump = D8;
const int button = D3;
float tempC;
float tempF;
float pressPA;
float pressinHg;
float humidRH;
int status;
int soilRead;

int pumpTimer;
bool buttonState;
int currentQual = -1;
unsigned long duration;
unsigned long startTime;
unsigned long sampleTime_ms = 30000;
unsigned long lowPulseOcc = 0;
float ratio = 0;
float concentration = 0;
char degree = 0xF8;
char perc = 0x25;
void DisplayRotation();
void WebPublish();



void setup() {
  Serial.begin(9600);
  pinMode(pump,OUTPUT);
  pinMode(dustSensor,INPUT);
  pinMode(button,INPUT);
  pinMode(soilSensor,INPUT);
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextColor(WHITE);

  WiFi.on();
  WiFi.connect();
  while(WiFi.connecting()) {
    Serial.printf(".");
  }
  Serial.printf("\n\n");


    status = bme.begin(0x76);
    if (status == false){
        Serial.printf("BME failed to start");
    }
}
void loop() {
 
  soilRead = analogRead(soilSensor);
  duration = pulseIn(dustSensor,LOW);
  lowPulseOcc = lowPulseOcc + duration;
  buttonState = digitalRead(button);
  tempC = bme.readTemperature();
  pressPA = bme.readPressure();
  humidRH = bme.readHumidity();
  tempF = (tempC*9/5)+32;
  pressinHg = pressPA/3386;

  Serial.printf("Moisture Level = %i\n", soilRead);
  if(soilRead>2000){
    Serial.printf("pump on");
    digitalWrite(pump,HIGH);
    delay(500);
    digitalWrite(pump,LOW);
  }
    
  if((millis()-pumpTimer)>500){
      Serial.printf("Pump Off");
      digitalWrite(pump,LOW);
      pumpTimer = millis();
  }

  if(buttonState){ 
    digitalWrite(pump,HIGH);
  }

  if(!buttonState && soilRead <= 2000){
    digitalWrite (pump,LOW);
  }

  

  if((millis()-startTime)>sampleTime_ms){
    ratio = lowPulseOcc/(sampleTime_ms*10.0); 
        concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; 
        Serial.printf("%i,%i,%i\n",lowPulseOcc,ratio,concentration);        
        lowPulseOcc = 0;
        startTime = millis();
  }  

   currentQual = aqSensor.slope();

  if (currentQual>= 0){
        if (currentQual ==3)
            Serial.printf("High pollution! Force signal active\n");
        else if (currentQual==2)
            Serial.printlnf("High pollution!\n");
        else if (currentQual==1)
            Serial.printf("Low pollution!\n");
        else if (currentQual ==0)
            Serial.printf("Fresh air\n");
  }

  DisplayRotation();
  WebPublish();
}

void WebPublish(){                       // publishes info to Adafruit
static int tempTimer;
static int soilTimer;
static int humidTimer;
static int aqTimer;

if((millis()-tempTimer > 9000)) {
    if(mqtt.Update()) {
      pubTemp.publish(tempF);
      Serial.printf("Publishing %0.2f \n",tempF); 
      } 
    tempTimer = millis();
  }

if((millis()-soilTimer > 10000)) {
    if(mqtt.Update()) {
      pubMoist.publish(soilRead);
      Serial.printf("Publishing soil moisture %i\n",soilRead); 
      } 
    soilTimer = millis();
  }

if((millis()-humidTimer > 11000)) {
    if(mqtt.Update()) {
      pubHumid.publish(humidRH);
      Serial.printf("Publishing soil moisture %i\n",soilRead); 
      } 
    humidTimer = millis();
  }

if((millis()-aqTimer > 12000)) {
    
    switch(currentQual){

      case 0:
       if(mqtt.Update()) {
          pubAQ.publish("Fresh Air");
       }
      break;

      case 1:
        if(mqtt.Update()) {
          pubAQ.publish("Low Pollution");
        }
      break;

      case 2:
        if(mqtt.Update()) {
            pubAQ.publish("High Pollution");
        }
        break;
      
      case 3:
         if(mqtt.Update()) {
            pubAQ.publish("POLLUTION AT DANGEROUS LEVELS");
         }
         break;

    }
  }
}

void DisplayRotation(){                // controls display read out

static int displayTimer;
static int displayCounter;



if((millis()-displayTimer)>2000){
  
  displayCounter++;
  display.clearDisplay();
  display.display();
  display.setCursor(0,0);
}

switch(displayCounter){

case 1:
  display.setTextSize(1);
  display.printf("Soil Moisture Level\n");
  display.setTextSize(3);
  display.printf("%i",soilRead);
  display.display();
  break;

case 2:
  display.setTextSize(1);
  display.printf("Current Temp\n");
  display.setTextSize(3);
  display.printf("%0.1f%cF",tempF,degree);
  display.display();
  break;

case 3:  
  display.setTextSize(1);
  display.printf("Humidity\n");
  display.setTextSize(3);
  display.printf("%0.1f%c",humidRH,perc);
  display.display(); 
  break;

case 4:
  display.setTextSize(1);
  display.printf("Air Pollution\n");
  display.display();
  display.setTextSize(2);
  if(currentQual == 0 ){
    display.printf("Safe");
    display.display();
  }
  if(currentQual == 1){
    display.printf("Low");
    display.display();
  }
  if(currentQual == 2){
    display.printf("High");
    display.display(); 
  }
  if(currentQual == 3){
    display.printf("DANGER!!");
    display.display();
  }
    displayCounter = 0;
    break;

}

}



          
        
    
 





