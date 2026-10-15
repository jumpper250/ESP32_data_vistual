# Firmware and flow checks

MQTT/DNS runs in a dedicated FreeRTOS task. Only that task accesses the
PubSubClient and WiFiClient after setup. The main loop reads the WiFi button,
sensor and LCD, and sends the newest telemetry through a one-item queue.
An atomic flag pauses MQTT processing while the WiFi setup portal is open.
The portal retains its existing 180-second timeout and restart behavior.

Relay commands must contain `cmd` equal to `pump` or `valve` and integer
`state` equal to `0` or `1`. Invalid requests do not change GPIOs or produce
a success ACK. Valid ACKs report the GPIO level after the write. The Node-RED
dashboard updates each relay switch from its ACK and periodic telemetry;
feedback messages update the UI only and are not republished as commands.

## Automated checks

- `platformio run` builds the ESP32 firmware.
- `node --test test/flows.test.cjs` checks flow IDs, references and routing.
- Build/run `test/relay_command_test.cpp` with a native C++17 compiler,
  adding `include` and `.pio/libdeps/esp32doit-devkit-v1/ArduinoJson/src`
  to the include path. This tests the same parser used by the firmware,
  without driving physical relays or publishing MQTT messages.

## Hardware checks after uploading

1. Start with the broker unavailable. Press and hold GPIO 4's button:
   `WiFi Config Mode` and the `parinya_AP` portal should appear while MQTT
   retries continue independently. Repeat with DNS unavailable.
2. Release the button. Configure WiFi and check that the portal closes,
   MQTT reconnects, subscriptions resume, and telemetry reaches Node-RED.
3. Confirm holding the button through portal completion does not reopen it
   until it has been released and pressed again.
4. With test loads disconnected, verify valid pump/valve commands and ACKs;
   invalid command names, missing fields, string/boolean states and values
   outside 0/1 must not change outputs or return a success ACK.

Automated tests do not substitute for these board and wiring checks.
