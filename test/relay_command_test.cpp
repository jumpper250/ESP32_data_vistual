#include "relay_command.h"
#include <cassert>
#include <iostream>
#include <string>

int main() {
  for (const char* name : {"pump", "valve"}) {
    for (int state : {0, 1}) {
      std::string json = std::string("{\"cmd\":\"") + name + "\",\"state\":" + std::to_string(state) + "}";
      RelayCommand result{};
      assert(parseRelayCommand(reinterpret_cast<const uint8_t*>(json.data()), json.size(), result));
      assert(result.state == state);
      assert(result.target == (std::string(name) == "pump" ? RelayTarget::Pump : RelayTarget::Valve));
    }
  }
  for (const char* invalid : {
      "", "{", "[]", "null", "{}", "{\"cmd\":\"pump\"}",
      "{\"state\":1}", "{\"cmd\":null,\"state\":1}",
      "{\"cmd\":42,\"state\":1}", "{\"cmd\":\"unknown\",\"state\":1}",
      "{\"cmd\":\"pump\",\"state\":-1}", "{\"cmd\":\"pump\",\"state\":2}",
      "{\"cmd\":\"pump\",\"state\":true}", "{\"cmd\":\"pump\",\"state\":null}",
      "{\"cmd\":\"pump\",\"state\":\"1\"}", "{\"cmd\":\"pump\",\"state\":0.5}",
      "{\"cmd\":\"pump\",\"state\":9999999999999999999}"}) {
    RelayCommand result{RelayTarget::Valve, 7};
    assert(!parseRelayCommand(reinterpret_cast<const uint8_t*>(invalid), strlen(invalid), result));
    assert(result.target == RelayTarget::Valve && result.state == 7);
  }
  const std::string oversized(257, ' ');
  RelayCommand result{};
  assert(!parseRelayCommand(reinterpret_cast<const uint8_t*>(oversized.data()), oversized.size(), result));
  const char bounded[] = "{\"cmd\":\"pump\",\"state\":1}garbage";
  assert(parseRelayCommand(reinterpret_cast<const uint8_t*>(bounded), strlen(bounded) - 7, result));
  std::cout << "PASS: valid commands, invalid types/ranges/JSON, payload bounds, and no result mutation on rejection\n";
}
