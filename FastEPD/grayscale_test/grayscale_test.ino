// 16-level grayscale test for the LilyGo T5 4.7" ESP32-S3 (960x540 ED047TC1).
// Draws 16 swatches (0 = black ... 15 = white) with their index underneath.
//
//
// The upstream sketch set a custom gray matrix and a 1872x1404 panel size for a
// different panel (T5 Pro); both are dropped here. The panel's built-in matrix is used.
#include <FastEPD.h>
#include "../lilygo_t5_47_s3.h"
T5EPD epaper;

#define SWATCH_W 60  // 16 * 60 = 960
#define SWATCH_H 300

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting...");

  int rc = epaper.initT5();
  if (rc != BBEP_SUCCESS) {
    Serial.printf("initPanel returned %d\n", rc);
    while (1) delay(1000);
  }
  epaper.setMode(BB_MODE_4BPP);
  epaper.fillScreen(0xf);
  for (int i = 0; i < 16; i++) {
    epaper.fillRect(i * SWATCH_W, 0, SWATCH_W, SWATCH_H, i);
  }
  epaper.drawRect(0, 0, 16 * SWATCH_W, SWATCH_H, 0);  // black outline around
  epaper.setFont(FONT_12x16);
  epaper.setTextColor(BBEP_BLACK);
  for (int i = 0; i < 16; i++) {
    epaper.setCursor(i * SWATCH_W + (i < 10 ? 24 : 18), SWATCH_H + 12);
    epaper.print(i, DEC);
  }
  epaper.fullUpdate(CLEAR_SLOW, false);
  Serial.println("Done");
  epaper.deInit();
}

void loop() {
  delay(1000);
}
