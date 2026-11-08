//
// Example to show a JPEG image
// as 16-gray levels
//
#include <FastEPD.h>
#include "../lilygo_t5_47_s3.h"
#include <JPEGDEC.h>
#include "mocha800x480.h"

// The image is 800x480; center it on the 960x540 panel. IMG_X must stay even
// (the draw callback packs two pixels per byte).
#define IMG_X 80
#define IMG_Y 30
JPEGDEC jpg;
T5EPD epaper;

int JPEGDraw(JPEGDRAW *pDraw)
{
  int x, y, iPitch = epaper.width()/2;
  uint8_t *s, *d, *pBuffer = epaper.currentBuffer();
  for (y=0; y<pDraw->iHeight; y++) {
    d = &pBuffer[((pDraw->y + y)*iPitch) + (pDraw->x/2)];
    s = (uint8_t *)pDraw->pPixels;
    s += (y * pDraw->iWidth);
    for (x=0; x<pDraw->iWidth; x+=2) {
        *d++ = (s[0] & 0xf0) | (s[1] >> 4);
        s += 2;
    } // for x
  } // for y
  return 1;
} /* JPEGDraw() */

void setup() {
  Serial.begin(115200);
  delay(3000); // wait for CDC-Serial to start
  // LilyGo T5 4.7" ESP32-S3 (960x540 ED047TC1). Uses ../lilygo_t5_47_s3.h (no FastEPD patch) with
  // FastEPD (see ../README.md).
  if (epaper.initT5() != BBEP_SUCCESS) {
    Serial.println("initPanel failed");
    return;
  }
  epaper.setMode(BB_MODE_4BPP);
  epaper.fillScreen(0xf);
  if (jpg.openFLASH((uint8_t *)mocha800x480, sizeof(mocha800x480), JPEGDraw)) {
      jpg.setPixelType(EIGHT_BIT_GRAYSCALE);
      jpg.decode(IMG_X, IMG_Y, 0);
      jpg.close();
      epaper.fullUpdate(CLEAR_SLOW, false);
      Serial.println("Done");
  } else {
      Serial.println("JPEG open failed");
  }
} /* setup() */

void loop() {
  delay(50000);
 // esp_sleep_enable_timer_wakeup(5000000);
 // esp_deep_sleep_start();

} /* loop() */
