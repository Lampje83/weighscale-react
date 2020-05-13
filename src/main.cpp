#include <DemoProject.h>
#include <ESP8266React.h>
#include <FS.h>
//#include <../TFT_eSPI_Setups/My_Setup_Select.h>
#include <TFT_eSPI.h>
//#include <GUIslice.h>
#include <ADC.h>

#define SERIAL_BAUD_RATE 115200

AsyncWebServer server(80);
ESP8266React esp8266React(&server, &SPIFFS);
DemoProject demoProject = DemoProject(&server, &SPIFFS, esp8266React.getSecurityManager());
TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);

void initADC(void* param) {
  ADC.begin(CS_PIN, DRDY_PIN);
  while (1) {
    ADC.loop();
  }
  // should never get here
  vTaskDelete(NULL);
}

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);

  // start the file system (must be done before starting the framework)
#ifdef ESP32
  SPIFFS.begin(true);
#elif defined(ESP8266)
  SPIFFS.begin();
#endif

  // start tft display
  tft.init();
  tft.fillScreen(TFT_WHITE);

  // start the framework and demo project
  esp8266React.begin();

  // start the demo project
  demoProject.begin();

  // start the ADS1220
  xTaskCreate(initADC, "initADC", 3000, NULL, 5, NULL);

  // start the server
  server.begin();
}

void loop() {
  // run the framework's loop function
  esp8266React.loop();

  // run the demo project's loop function
  demoProject.loop();
}
