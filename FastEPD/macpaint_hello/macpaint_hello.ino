//
// MacPaint "hello" handwriting animation with partial updates.
//
// Ported from the Good Display GDEQ0426T82 / XIAO MG24 sketch GDEQ0426T82_MacPaint
// (SSD1677, 800x480 monochrome), itself a port of upiir's
// https://github.com/upiir/seeed_eink_partial_update "hello" animation.
// Here: LilyGo T5 4.7" ESP32-S3 (960x540) with FastEPD. Uses ../lilygo_t5_47_s3.h (no FastEPD patch) with
// FastEPD (see ../README.md).
//
// The 640x480 MacPaint bitmap (image.h) is drawn once with a full refresh, then a brush follows the
// recorded pen path (positions.h) and only the pixels that changed are refreshed.
// It runs once and stops; press RST to replay.
//
#include <FastEPD.h>
#include "../lilygo_t5_47_s3.h"
#include "image.h"       // gImage_macos_paint_640x480, 1 bit per pixel, 1 = black
#include "positions.h"   // pen path in 800x480 coordinates

T5EPD epaper;

// The original canvas is 800x480; center that frame on the 960x540 panel.
#define FRAME_X       80
#define FRAME_Y       30
// The 640x480 bitmap sat at x=80 inside the 800x480 frame.
#define IMG_X         (FRAME_X + 80)
#define IMG_Y         FRAME_Y
#define IMG_W         640
#define IMG_H         480
#define BRUSH_RADIUS  4

int animIndex = 0;

void drawBackground()
{
  epaper.fillScreen(BBEP_WHITE);
  for (int row = 0; row < IMG_H; row++) {
    for (int col = 0; col < IMG_W; col++) {
      uint8_t b = pgm_read_byte(&gImage_macos_paint_640x480[row * (IMG_W / 8) + (col / 8)]);
      if (b & (0x80 >> (col & 7))) epaper.drawPixel(IMG_X + col, IMG_Y + row, BBEP_BLACK);
    }
  }
  epaper.fullUpdate(CLEAR_SLOW, true);   // keeps the previous-frame state that partialUpdate() needs
}

void setup()
{
  Serial.begin(115200);
  epaper.initT5();
  epaper.setPasses(3);                   // fewer passes = faster partial updates
  epaper.clearWhite();
  epaper.fullUpdate(CLEAR_NONE, true);   // required once before partialUpdate()
  drawBackground();
  Serial.println("Background drawn");
}

void loop()
{
  if (animIndex < NUM_POSITIONS) {
    int x = FRAME_X + (int16_t)pgm_read_word(&positions[animIndex][0]);
    int y = FRAME_Y + (int16_t)pgm_read_word(&positions[animIndex][1]);
    epaper.fillCircle(x, y, BRUSH_RADIUS, BBEP_BLACK);
    // Do not pass a row range here: FastEPD 2.2.0 leaves stale data in a DMA buffer for the skipped rows,
    // which draws gray vertical streaks. The previous/current diff already leaves unchanged pixels alone.
    epaper.partialUpdate(true);
    animIndex++;
    if (animIndex % 25 == 0) Serial.printf("%d / %d points\n", animIndex, NUM_POSITIONS);
  } else {
    Serial.println("Done, stopping");
    epaper.deInit();   // shut down the power controller and I/O expander; the image stays on the panel
    while (1) delay(1000);
  }
}
