# ESPUpdate

A library for updating ESP firmware over the air.

## Installation

1. Download the repository.
2. Copy the `ESPUpdate` folder to your Arduino `libraries` folder.

## Usage

```cpp
#include <ESPUpdate.h>

void setup() {
  ESPUpdate.begin();
}

void loop() {
  ESPUpdate.handleClient();
}
