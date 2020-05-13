#include <SystemStatus.h>
#include "esp_adc_cal.h"

esp_adc_cal_characteristics_t adc_chars;

SystemStatus::SystemStatus(AsyncWebServer* server, SecurityManager* securityManager) {
  server->on(SYSTEM_STATUS_SERVICE_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&SystemStatus::systemStatus, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
  pinMode(VBAT_PIN, ANALOG);
  //adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, 1100, &adc_chars);
}

void SystemStatus::systemStatus(AsyncWebServerRequest* request) {
  AsyncJsonResponse* response = new AsyncJsonResponse(false, MAX_ESP_STATUS_SIZE);
  JsonObject root = response->getRoot();
#ifdef ESP32
  root["esp_platform"] = "esp32";
#elif defined(ESP8266)
  root["esp_platform"] = "esp8266";
#endif
  root["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
  root["free_heap"] = ESP.getFreeHeap();
  root["sketch_size"] = ESP.getSketchSize();
  root["free_sketch_space"] = ESP.getFreeSketchSpace();
  root["sdk_version"] = ESP.getSdkVersion();
  root["flash_chip_size"] = ESP.getFlashChipSize();
  root["flash_chip_speed"] = ESP.getFlashChipSpeed();
  root["battery_voltage"] = esp_adc_cal_raw_to_voltage(analogRead(VBAT_PIN), &adc_chars) * 2;
  uint32_t milliV;
  //esp_adc_cal_get_voltage(ADC_CHANNEL_6, &adc_chars, &milliV);
  //root["battery_voltage"] = milliV;
  response->setLength();
  request->send(response);
}
