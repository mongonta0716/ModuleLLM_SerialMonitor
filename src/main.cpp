#include <Arduino.h>
#include <M5Unified.h>
#include "AquesTalkTTS.h"
#include <Avatar.h>
#include "LipSync.h"

using namespace m5avatar;

Avatar avatar;

void setup() { 
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.setTouchButtonHeight(40);
    M5.Display.setFont(&fonts::lgfxJapanMincho_16);
    M5.Lcd.print("Hello, M5Stack!");
    Serial2.begin(115200, SERIAL_8N1, 18, 17);

    avatar.init();
    avatar.addTask(lipSync, "lipSync");
    // AquesTalkの初期化
//    uint8_t mac[6];
//    esp_efuse_mac_get_default(mac);
//    M5_LOGI("¥nMAC Address = %02X:%02X:%02X:%02X:%02X:%02X¥r¥n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    int err = TTS.createK();
    if (err) {
        M5_LOGI("ERR:TTS.createK(): %d", err);
    }
    M5.Speaker.setVolume(128);
    TTS.playK("私はスタックチャン。ボタンを押してね！");
    TTS.wait();   // 終了まで待つ



}

void loop() {
    M5.update();
    if (M5.BtnA.wasClicked()) {
        Serial2.println("start");
        M5.Speaker.tone(1000, 500);
        M5_LOGI("start send");
    }
    if (Serial2.available()) {
        String input = Serial2.readString();
        //M5.Display.println(input);
        TTS.playK(input.c_str());
        TTS.wait();
        Serial2.println("end");
    }
}