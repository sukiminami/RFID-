#include <Arduino.h>
#include "SystemManager.h"

SystemManager systemManager;

void setup() {
    Serial0.begin(115200);
    delay(2000);
    
    systemManager.begin();
}

void loop() {
    systemManager.loop();
}