#include <fermentation-chamber/ChamberSettingsService.h>

ChamberSettingsService::ChamberSettingsService(AsyncWebServer* server, FS* fs) :
  SettingsService(server, fs, CHAMBER_SETTINGS_SERVICE_PATH, CHAMBER_SETTINGS_FILE) {
    server->on(CHAMBER_STATUS_SERVICE_PATH, HTTP_GET, std::bind(&ChamberSettingsService::chamberStatus, this, std::placeholders::_1));
  }

ChamberSettingsService::~ChamberSettingsService() {}

void ChamberSettingsService::begin() {
  // load settings
  SettingsService::begin();

  // set both pins to output
  pinMode(COOLER_PIN, OUTPUT);
  pinMode(HEATER_PIN, OUTPUT);

  // configure to idle mode
  transitionToStatus(STATUS_IDLE);

  Serial.print("Starting temp sensor bus...");
  _tempSensors.begin();
  _tempSensors.setResolution(12);
  _tempSensors.setWaitForConversion(false);
  Serial.println("finished!");

  Serial.print("Found ");
  Serial.print(_tempSensors.getDS18Count());
  Serial.println(" sensor(s).");

  // pre-calculate threshold temps
  onConfigUpdated();

  // request temps
  prepareNextControllerLoop();
}

void ChamberSettingsService::prepareNextControllerLoop() {
  _tempSensors.requestTemperatures();
  _nextEvaluation = millis() + TEMP_SENSOR_INTERVAL;
}

void ChamberSettingsService::changeStatus(uint8_t newStatus, unsigned long *previousToggle, unsigned long *toggleLimitDuration) {
  unsigned long currentMillis = millis();
  unsigned long toggleElapsed = (unsigned long)(currentMillis - *previousToggle);
  if (toggleElapsed >= *toggleLimitDuration){
    *previousToggle = currentMillis;
    transitionToStatus(newStatus);
  }
}

void ChamberSettingsService::transitionToStatus(uint8_t newStatus) {
  _status = newStatus;
  switch (_status) {
    case STATUS_HEATING:
      digitalWrite(COOLER_PIN, LOW);
      digitalWrite(HEATER_PIN, HIGH);
      break;
    case STATUS_COOLING:
      digitalWrite(COOLER_PIN, HIGH);
      digitalWrite(HEATER_PIN, LOW);
      break;
    case STATUS_IDLE:
    default:
      digitalWrite(COOLER_PIN, LOW);
      digitalWrite(HEATER_PIN, LOW);
      break;
  }
}

void ChamberSettingsService::loop() {
  // exit eagerly if we have no processing to do.
  if (millis() < _nextEvaluation) {
    return;
  }

  float temp = _tempSensors.getTempC(_chamberSensorAddress);
  Serial.print("Temp is currently:");
  Serial.println(temp);
  Serial.print("Status is currently:");
  Serial.println(_status);

  switch (_status) {
    case STATUS_HEATING:
      if (!_enableHeater || temp == DEVICE_DISCONNECTED_C || temp >= _heaterOffTemp){
        changeStatus(STATUS_IDLE, &_heaterToggledAt, &_minHeaterOnDuration);
      }
      break;
    case STATUS_COOLING:
      if (!_enableCooler || temp == DEVICE_DISCONNECTED_C || temp <= _coolerOffTemp){
        changeStatus(STATUS_IDLE, &_coolerToggledAt, &_minCoolerOnDuration);
      }
      break;
    case STATUS_IDLE:
    default:
      if (temp != DEVICE_DISCONNECTED_C){
        if (_enableHeater && temp <= _heaterOnTemp){
          changeStatus(STATUS_HEATING, &_heaterToggledAt, &_minHeaterOffDuration);
        }
        if (_enableCooler && temp >= _coolerOnTemp){
          changeStatus(STATUS_COOLING, &_coolerToggledAt, &_minCoolerOffDuration);
        }
      }
  }

  prepareNextControllerLoop();
}

void ChamberSettingsService::onConfigUpdated() {
  _heaterOffTemp = _targetTemp - (_hysteresisLow * _hysteresisFactor);
  _coolerOffTemp = _targetTemp + (_hysteresisHigh * _hysteresisFactor);
  _heaterOnTemp = _targetTemp - _hysteresisLow;
  _coolerOnTemp = _targetTemp + _hysteresisHigh;
}

void ChamberSettingsService::configureController() {
  // set next evaluation to be immediately fired
  _nextEvaluation = millis();
}

void ChamberSettingsService::readFromJsonObject(JsonObject& root) {
   deviceAddressFromString(root["chamber_sensor_address"], _chamberSensorAddress);
   deviceAddressFromString(root["ambient_sensor_address"], _ambientSensorAddress);

  _targetTemp = root["target_temp"];
  _hysteresisHigh = root["hysteresis_high"];
  _hysteresisLow = root["hysteresis_low"];
  _hysteresisFactor = root["hysteresis_factor"];

  _minHeaterOnDuration = root["min_heater_on_duration"];
  _minHeaterOffDuration = root["min_heater_off_duration"];
  _minCoolerOnDuration = root["min_cooler_on_duration"];
  _minCoolerOffDuration = root["min_cooler_off_duration"];

  _enableHeater = root["enable_heater"];
  _enableCooler = root["enable_cooler"];
}

void ChamberSettingsService::writeToJsonObject(JsonObject& root) {
  root["chamber_sensor_address"] = deviceAddressAsString(_chamberSensorAddress);
  root["ambient_sensor_address"] = deviceAddressAsString(_ambientSensorAddress);

  root["target_temp"] = _targetTemp;
  root["hysteresis_high"] = _hysteresisHigh;
  root["hysteresis_low"] = _hysteresisLow;
  root["hysteresis_factor"] = _hysteresisFactor;

  root["min_heater_on_duration"] = _minHeaterOnDuration;
  root["min_heater_off_duration"] = _minHeaterOffDuration;
  root["min_cooler_on_duration"] = _minCoolerOnDuration;
  root["min_cooler_off_duration"] = _minCoolerOffDuration;

  root["enable_heater"] = _enableHeater;
  root["enable_cooler"] = _enableCooler;
}

void ChamberSettingsService::chamberStatus(AsyncWebServerRequest *request) {
  AsyncJsonResponse * response = new AsyncJsonResponse();
  JsonObject& root = response->getRoot();

  // basic config info - for display on status page
  root["chamber_sensor_address"] = deviceAddressAsString(_chamberSensorAddress);
  root["ambient_sensor_address"] = deviceAddressAsString(_ambientSensorAddress);
  root["target_temp"] = _targetTemp;
  root["enable_heater"] = _enableHeater;
  root["enable_cooler"] = _enableCooler;

  // temp thresholds
  root["heater_on_temp"] = _heaterOnTemp;
  root["cooler_on_temp"] = _coolerOnTemp;
  root["heater_off_temp"] = _heaterOffTemp;
  root["cooler_off_temp"] = _coolerOffTemp;

  // current status
  root["status"] = _status;

  // write out sensors and current readings
  JsonObject& sensors = root.createNestedObject("sensors");
  DeviceAddress address;
  for (uint8_t i = 0; i < _tempSensors.getDS18Count(); i++) {
    if (_tempSensors.getAddress(address, i)) {
      JsonObject& sensorDetails = sensors.createNestedObject(deviceAddressAsString(address));
      sensorDetails["temp_c"] =  _tempSensors.getTempC(address);
    }
  }

  // send response
  response->setLength();
  request->send(response);
}