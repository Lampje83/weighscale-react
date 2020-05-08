#ifndef SettingsService_h
#define SettingsService_h

#include <Arduino.h>
#include <SettingsDeserializer.h>
#include <SettingsSerializer.h>

#include <list>
#include <functional>
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

typedef size_t update_handler_id_t;
typedef std::function<void(String originId)> SettingsUpdateCallback;
static update_handler_id_t currentUpdatedHandlerId;

typedef struct SettingsUpdateHandlerInfo {
  update_handler_id_t _id;
  SettingsUpdateCallback _cb;
  bool _allowRemove;
  SettingsUpdateHandlerInfo(SettingsUpdateCallback cb, bool allowRemove) :
      _id(++currentUpdatedHandlerId), _cb(cb), _allowRemove(allowRemove){};
} SettingsUpdateHandlerInfo_t;

template <class T>
class SettingsService {
 public:
  template <typename... Args>
#ifdef ESP32
  SettingsService(Args&&... args) :
      _settings(std::forward<Args>(args)...), _updateMutex(xSemaphoreCreateRecursiveMutex()) {
  }
#else
  SettingsService(Args&&... args) : _settings(std::forward<Args>(args)...) {
  }
#endif

  update_handler_id_t addUpdateHandler(SettingsUpdateCallback cb, bool allowRemove = true) {
    if (!cb) {
      return 0;
    }
    SettingsUpdateHandlerInfo_t updateHandler(cb, allowRemove);
    _settingsUpdateHandlers.push_back(updateHandler);
    return updateHandler._id;
  }

  void removeUpdateHandler(update_handler_id_t id) {
    for (auto i = _settingsUpdateHandlers.begin(); i != _settingsUpdateHandlers.end();) {
      if ((*i)._allowRemove && (*i)._id == id) {
        i = _settingsUpdateHandlers.erase(i);
      } else {
        ++i;
      }
    }
  }

  void updateWithoutPropagation(std::function<void(T&)> callback) {
#ifdef ESP32
    xSemaphoreTakeRecursive(_updateMutex, portMAX_DELAY);
#endif
    callback(_settings);
#ifdef ESP32
    xSemaphoreGiveRecursive(_updateMutex);
#endif
  }

  void updateWithoutPropagation(JsonObject& jsonObject, SettingsDeserializer<T> deserializer) {
#ifdef ESP32
    xSemaphoreTakeRecursive(_updateMutex, portMAX_DELAY);
#endif
    deserializer(jsonObject, _settings);
#ifdef ESP32
    xSemaphoreGiveRecursive(_updateMutex);
#endif
  }

  void update(std::function<void(T&)> callback, String originId) {
#ifdef ESP32
    xSemaphoreTakeRecursive(_updateMutex, portMAX_DELAY);
#endif
    callback(_settings);
    callUpdateHandlers(originId);
#ifdef ESP32
    xSemaphoreGiveRecursive(_updateMutex);
#endif
  }

  void update(JsonObject& jsonObject, SettingsDeserializer<T> deserializer, String originId) {
#ifdef ESP32
    xSemaphoreTakeRecursive(_updateMutex, portMAX_DELAY);
#endif
    deserializer(jsonObject, _settings);
    callUpdateHandlers(originId);
#ifdef ESP32
    xSemaphoreGiveRecursive(_updateMutex);
#endif
  }

  void read(std::function<void(T&)> callback) {
#ifdef ESP32
    xSemaphoreTakeRecursive(_updateMutex, portMAX_DELAY);
#endif
    callback(_settings);
#ifdef ESP32
    xSemaphoreGiveRecursive(_updateMutex);
#endif
  }

  void read(JsonObject& jsonObject, SettingsSerializer<T> serializer) {
#ifdef ESP32
    xSemaphoreTakeRecursive(_updateMutex, portMAX_DELAY);
#endif
    serializer(_settings, jsonObject);
#ifdef ESP32
    xSemaphoreGiveRecursive(_updateMutex);
#endif
  }

  void callUpdateHandlers(String originId) {
    for (const SettingsUpdateHandlerInfo_t& handler : _settingsUpdateHandlers) {
      handler._cb(originId);
    }
  }

 protected:
  T _settings;
#ifdef ESP32
  SemaphoreHandle_t _updateMutex;
#endif
 private:
  std::list<SettingsUpdateHandlerInfo_t> _settingsUpdateHandlers;
};

#endif  // end SettingsService_h
