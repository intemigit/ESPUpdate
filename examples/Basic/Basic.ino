#include <ESPUpdate.h>

void setup() {
  ESPUpdate.begin();
}

void loop() {
  ESPUpdate.handleClient();
}