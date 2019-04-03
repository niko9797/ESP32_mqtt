#include <Arduino.h>
#include <Wire.h>
#include "Ticker.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
//https://github.com/beegee-tokyo/DHTesp/blob/master/examples/DHT_ESP32/DHT_ESP32.ino
#include "SSD1306.h"
#include "OLEDDisplayUi.h"
#include "images.h"
//https://github.com/ThingPulse/esp8266-oled-ssd1306

// Insert Wifi and MQTT settings
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
// Pin number for DHT data pin
int dhtPin1 = 14;
int dhtPin2 = 27;
// Change with screen settings
SSD1306  display(0x3c, 21, 22);
OLEDDisplayUi ui     ( &display );
// MQTT relay topic
const char* relay_topic = "cuarto/balcon/test";
// MQTT DHT topics
const char* dht_1_h_topic = "cuarto/balcon/arriba_h";
const char* dht_1_t_topic = "cuarto/balcon/arriba_t";
const char* dht_2_h_topic = "cuarto/balcon/abajo_h";
const char* dht_2_t_topic = "cuarto/balcon/abajo_t";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
int ledPin = 2;


/** Initialize DHT sensor 1 */
DHTesp dhtSensor1;
/** Initialize DHT sensor 2 */
DHTesp dhtSensor2;
/** Task handle for the light value read task */
TaskHandle_t tempTaskHandle = NULL;
/** Task handle for the light value read task */
TaskHandle_t mqttTaskHandle = NULL;
/** Ticker for temperature reading */
Ticker tempTicker;
/** Ticker for temperature reading */
Ticker mqttTicker;
/** Flags for temperature readings finished */
bool gotNewTemperature = false;
/** Data from sensor 1 */
TempAndHumidity sensor1Data;
/** Data from sensor 2 */
TempAndHumidity sensor2Data;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == relay_topic) {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe to topic, add more statements if needed
      client.subscribe(relay_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishData() {

    if (!client.connected()) {
    setup_wifi();
    reconnect();
  }
    char tempString[8];
    String(sensor1Data.temperature).toCharArray(tempString, 8);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish(dht_1_t_topic, tempString);
    
    // Convert the value to a char array
    String(sensor1Data.humidity).toCharArray(tempString, 8);
    Serial.print("Humidity: ");
    Serial.println(tempString);
    client.publish(dht_1_h_topic, tempString);


    String(sensor2Data.temperature).toCharArray(tempString, 8);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish(dht_2_t_topic, tempString);
    
    // Convert the value to a char array
    String(sensor2Data.humidity).toCharArray(tempString, 8);
    Serial.print("Humidity: ");
    Serial.println(tempString);
    client.publish(dht_2_h_topic, tempString);
}

void mqttTask(void *pvParameters) {

  while (1) // mqttTaskloop
	{
    publishData();
		vTaskSuspend(NULL);
  }
}



void tempTask(void *pvParameters) {
	Serial.println("tempTask loop started");
	while (1) // tempTask loop
	{
		//if (tasksEnabled) { // Read temperature only if old data was processed already
			// Reading temperature for humidity takes about 250 milliseconds!
			// Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
			sensor1Data = dhtSensor1.getTempAndHumidity();	// Read values from sensor 1
			sensor2Data = dhtSensor2.getTempAndHumidity();	// Read values from sensor 1
			gotNewTemperature = true;
      Serial.print("temp updated");
		//}
		vTaskSuspend(NULL);
	}
}

void triggerGetTemp() {
	if (tempTaskHandle != NULL) {
		 xTaskResumeFromISR(tempTaskHandle);
	}
}

void triggermqtt() {
	if (mqttTaskHandle != NULL) {
		 xTaskResumeFromISR(mqttTaskHandle);
	}
}



void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, String(millis()));
}

void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // draw an xbm image.
  // Please note that everything that should be transitioned
  // needs to be drawn relative to x and y

  if (client.connected()) {
  display->drawXbm(35 + x, 10 + y, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  }
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_24);
  display->drawString(20 + x, 10 + y, String(sensor1Data.temperature,2) + " ºC");
  display->drawString(20 + x, 35 + y, String(sensor1Data.humidity,2) + " %");
}

void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_24);
  display->drawString(20 + x, 10 + y, String(sensor2Data.temperature,2) + " ºC");
  display->drawString(20 + x, 35 + y, String(sensor2Data.humidity,2) + " %");
}

FrameCallback frames[] = { drawFrame1, drawFrame2, drawFrame3};

// how many frames are there?
int frameCount = 3;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("DHT ESP32 temp and humidity MQTT node with tasks");
  Serial.println("Niko Rodriguez 02/2019");

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

    // Initialize temperature sensor 1
	dhtSensor1.setup(dhtPin1, DHTesp::DHT22);
	// Initialize temperature sensor 2
	dhtSensor2.setup(dhtPin2, DHTesp::DHT22);
    

    


// Start task to get temperature
	xTaskCreate(
			tempTask,											 /* Function to implement the task */
			"tempTask ",										/* Name of the task */
			4000,													 /* Stack size in words */
			NULL,													 /* Task input parameter */
			6,															/* Priority of the task */
			&tempTaskHandle);

  xTaskCreate(
			mqttTask,											 /* Function to implement the task */
			"mqttTask",										/* Name of the task */
			4000,													 /* Stack size in words */
			NULL,													 /* Task input parameter */
			5,															/* Priority of the task */
			&mqttTaskHandle);
	

if (tempTaskHandle == NULL) {
		Serial.println("[ERROR] Failed to start task for temperature update");
	} else {
		// Start update of environment data every 30 seconds
		tempTicker.attach(10, triggerGetTemp);
	}

if (mqttTaskHandle == NULL) {
		Serial.println("[ERROR] Failed to start task for mqtt update");
	} else {
		// Start update of environment data every 30 seconds
		mqttTicker.attach(30, triggermqtt);
	}

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);


  display.init();
  display.display();
  // The ESP is capable of rendering 60fps in 80Mhz mode
	// but that won't give you much time for anything else
	// run it in 160Mhz mode or just set it to 30 fps
  ui.setTargetFPS(30);

	// Customize the active and inactive symbol
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  //ui.setOverlays(overlays, overlaysCount);

  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();
}

void loop() {
    int remainingTimeBudget = ui.update();
    
    if (remainingTimeBudget > 0) {
      client.loop();
        
        if (gotNewTemperature) {
                Serial.println("Sensor 1 data:");
                Serial.println("Temp: " + String(sensor1Data.temperature,2) + "'C Humidity: " + String(sensor1Data.humidity,1) + "%");
                Serial.println("Sensor 2 data:");
                Serial.println("Temp: " + String(sensor2Data.temperature,2) + "'C Humidity: " + String(sensor2Data.humidity,1) + "%");
                gotNewTemperature = false;
            }
        delay(remainingTimeBudget);
    };
} 

 
