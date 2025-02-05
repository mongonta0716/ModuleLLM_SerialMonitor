#include <Arduino.h>
#include <M5Unified.h>
#include "AquesTalkTTS.h"

void setup() { 
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setFont(&fonts::lgfxJapanMincho_16);
    M5.Lcd.print("Hello, M5Stack!");
    Serial2.begin(115200, SERIAL_8N1, 18, 17);
    // AquesTalkの初期化
//    uint8_t mac[6];
//    esp_efuse_mac_get_default(mac);
//    M5_LOGI("¥nMAC Address = %02X:%02X:%02X:%02X:%02X:%02X¥r¥n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    int err = TTS.createK();
    if (err) {
        M5_LOGI("ERR:TTS.createK(): %d", err);
    }
    M5.Speaker.setVolume(128);
    TTS.playK("こんにちは、漢字を混ぜた文章です。");
    TTS.wait();   // 終了まで待つ



}

void loop() {
    if (Serial2.available()) {
        String input = Serial2.readString();
        M5.Display.println(input);
        TTS.playK(input.c_str());
        TTS.wait();
    }
}