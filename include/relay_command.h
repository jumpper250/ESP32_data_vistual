#pragma once

#include <ArduinoJson.h>
#include <cstring>

enum class RelayTarget { Pump, Valve };

struct RelayCommand {
  RelayTarget target;
  int state;
};

// Validate the entire request before changing any GPIO or sending an ACK.
inline bool parseRelayCommand(const uint8_t* payload, size_t length, RelayCommand& result) {
  if (length == 0 || length > 256) return false;
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return false;
  if (!doc["cmd"].is<const char*>() || !doc["state"].is<int>()) return false;

  const char* command = doc["cmd"].as<const char*>();
  int state = doc["state"].as<int>();
  if (state != 0 && state != 1) return false;
  RelayTarget target;
  if (strcmp(command, "pump") == 0) target = RelayTarget::Pump;
  else if (strcmp(command, "valve") == 0) target = RelayTarget::Valve;
  else return false;

  result = {target, state};
  return true;
}
