#include <Arduino.h>
#include <M5Unified.h>

void setup() { 
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setFont(&fonts::lgfxJapanMincho_16);
    M5.Lcd.print("Hello, M5Stack!");
    Serial2.begin(115200, SERIAL_8N1, 18, 17);

}

void loop() {
    if (Serial2.available()) {
        String input = Serial2.readString();
        M5.Display.println(input);
    }
}