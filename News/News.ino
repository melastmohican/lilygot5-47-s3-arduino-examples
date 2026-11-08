// News: top headlines from NewsAPI on the LilyGo T5 4.7" ESP32-S3 (960x540, 16 grays).
//
// Ported from the Seeed reTerminal E1002 "News" sketch (GxEPD2, 800x480 six-colour), which was
// ported from the Soldered Inkplate 6COLOR "News" example.
// Setup: copy secret.h.example to secret.h and set WiFi plus a free NewsAPI key from https://newsapi.org/ .
//
// Arduino Setting
//            Tools ->
//                  Board:"ESP32S3 Dev Module"
//                  USB CDC On Boot:"Enable"
//                  USB DFU On Boot:"Disable"
//                  Flash Size : "16MB(128Mb)"
//                  Flash Mode"QIO 80MHz
//                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
//                  PSRAM:"OPI PSRAM"
//                  Upload Mode:"UART0/Hardware CDC"
//                  USB Mode:"Hardware CDC and JTAG"
//
// With a USB host connected the sketch stays awake and refreshes every DevRefreshMinutes;
// otherwise it deep-sleeps SleepMinutes between refreshes. DEV_NO_SLEEP forces staying awake.

//#define DEV_NO_SLEEP
#define DevRefreshMinutes 30   // NewsAPI free tier allows 100 requests/day
#define SleepMinutes      60

#include <Arduino.h>
#include "epd_driver.h"   // https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secret.h"       // WIFI_SSID, WIFI_PASSWORD, NEWS_API_KEY (gitignored)

#include "opensans8b.h"
#include "opensans10b.h"
#include "opensans12b.h"
#include "opensans18b.h"
#include "opensans24b.h"

// ---------- CHANGE HERE ----------
const char *timezone = "PST8PDT,M3.2.0,M11.1.0";   // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char *newsCountry = "us";                      // NewsAPI country code
// ---------------------------------

#define MAX_ITEMS 4
struct NewsItem { String title; String description; };
NewsItem items[MAX_ITEMS];
int itemCount = 0;

// ===== Display helpers (LilyGo-EPD47 framebuffer, gray levels) ==============
#define White      0xFF
#define LightGrey  0xBB
#define Grey       0x88
#define DarkGrey   0x44
#define Black      0x00

uint8_t *framebuffer;
GFXfont currentFont;

static void setFont(GFXfont const &font) { currentFont = font; }
static void clearFrame() { memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2); }

// Draw text with its baseline at y.
static void drawText(int x, int y, const String &text) {
  int32_t cx = x, cy = y;
  write_string(&currentFont, const_cast<char *>(text.c_str()), &cx, &cy, framebuffer);
}

static int textWidth(const String &text) {
  int32_t x = 0, y = 0, x1, y1, w, h;
  get_text_bounds(&currentFont, const_cast<char *>(text.c_str()), &x, &y, &x1, &y1, &w, &h, NULL);
  return w;
}

static void drawTextRight(int xRight, int y, const String &text) { drawText(xRight - textWidth(text), y, text); }
static void drawTextCentered(int xCenter, int y, const String &text) { drawText(xCenter - textWidth(text) / 2, y, text); }

// GFX fonts are ASCII only: replace common UTF-8 punctuation, drop everything else non-ASCII.
static void asciiFold(String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length();) {
    uint8_t c = s[i];
    if (c < 0x80) { out += (char)c; i++; continue; }
    if (c == 0xE2 && i + 2 < s.length()) {
      uint8_t b = s[i + 2];
      if (s[i + 1] == (char)0x80) {
        if (b == 0x98 || b == 0x99) out += '\'';
        else if (b == 0x9C || b == 0x9D) out += '"';
        else if (b == 0x93 || b == 0x94) out += '-';
        else if (b == 0xA6) out += "...";
      }
      i += 3;
      continue;
    }
    i += (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;   // skip the rest of a multi-byte sequence
  }
  s = out;
}

// Word-wrap text into a box of width w starting at baseline y with the current font.
// Draws at most maxLines lines ("..." on overflow). Returns the baseline for the next line.
static int drawWrapped(int x, int y, int w, int maxLines, const String &text, int lineHeight) {
  int line = 0, pos = 0, n = text.length();
  String cur;
  while (pos < n && line < maxLines) {
    int sp = text.indexOf(' ', pos);
    String word = (sp < 0) ? text.substring(pos) : text.substring(pos, sp);
    pos = (sp < 0) ? n : sp + 1;
    String trial = cur.length() ? cur + " " + word : word;
    if (textWidth(trial) > w && cur.length()) {
      drawText(x, y + line * lineHeight, cur);
      line++;
      cur = word;
    } else {
      cur = trial;
    }
  }
  if (line < maxLines && cur.length()) {
    drawText(x, y + line * lineHeight, cur);
    line++;
  } else if (pos < n && line >= maxLines) {
    drawText(x + w - 30, y + (maxLines - 1) * lineHeight, "...");
  }
  return y + line * lineHeight;
}

static void pushFrame() {
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff_all();
}

static void drawMessage(const char *msg) {
  clearFrame();
  setFont(OpenSans18B);
  drawText(60, 280, msg);
  pushFrame();
}

static bool connectWiFi(unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

// ===== Power =================================================================
static bool StayAwake() {
#ifdef DEV_NO_SLEEP
  return true;
#elif ARDUINO_USB_MODE == 1 && ARDUINO_USB_CDC_ON_BOOT
  return Serial.isConnected();
#else
  return false;
#endif
}

static void BeginSleep() {
  epd_poweroff_all();
  if (StayAwake()) {
    Serial.println("USB connected: staying awake, next refresh in " + String(DevRefreshMinutes) + " min");
    return;
  }
  esp_sleep_enable_timer_wakeup((uint64_t)SleepMinutes * 60ULL * 1000000ULL);
  Serial.println("Deep sleep for " + String(SleepMinutes) + " min");
  Serial.flush();
  esp_deep_sleep_start();
}

// ===== Network ================================================================
static bool fetchNews() {
  WiFiClientSecure client;
  client.setInsecure();   // public headlines only; no certificate pinning
  HTTPClient http;
  String url = String("https://newsapi.org/v2/top-headlines?country=") + newsCountry +
               "&pageSize=10&apiKey=" + NEWS_API_KEY;
  if (!http.begin(client, url)) return false;
  http.addHeader("User-Agent", "LilyGo-T5-4.7");
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[news] HTTP %d\n", code);
    http.end();
    return false;
  }
  JsonDocument filter;   // keep only what we draw to save RAM
  filter["articles"][0]["title"] = true;
  filter["articles"][0]["description"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("[news] JSON error: %s\n", err.c_str());
    return false;
  }
  itemCount = 0;
  for (JsonObject a : doc["articles"].as<JsonArray>()) {
    if (itemCount >= MAX_ITEMS) break;
    const char *t = a["title"];
    const char *d = a["description"];
    if (!t || !d || !*d) continue;   // skip articles without a description
    items[itemCount].title = t;
    items[itemCount].description = d;
    asciiFold(items[itemCount].title);
    asciiFold(items[itemCount].description);
    itemCount++;
  }
  return itemCount > 0;
}

// ===== Drawing ================================================================
static void drawNews() {
  const int W = EPD_WIDTH, margin = 40;

  struct tm t;
  getLocalTime(&t, 5000);
  char dateStr[32], updStr[32];
  snprintf(dateStr, sizeof(dateStr), "Date: %02d.%02d.%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  snprintf(updStr, sizeof(updStr), "Last update: %02d:%02d", t.tm_hour, t.tm_min);

  clearFrame();
  setFont(OpenSans24B);
  drawTextCentered(W / 2, 48, "World News");
  epd_fill_rect(margin, 62, W - 2 * margin, 4, DarkGrey, framebuffer);

  setFont(OpenSans10B);
  drawText(margin, 92, dateStr);
  drawTextRight(W - margin, 92, updStr);
  epd_fill_rect(margin, 102, W - 2 * margin, 2, Black, framebuffer);

  const int top = 112, boxH = 105;
  for (int i = 0; i < itemCount; i++) {
    int y = top + i * boxH;
    setFont(OpenSans12B);
    int next = drawWrapped(margin, y + 24, W - 2 * margin, 2, items[i].title, 28);
    setFont(OpenSans8B);
    drawWrapped(margin, next + 4, W - 2 * margin, 2, items[i].description, 22);
    if (i < itemCount - 1) epd_draw_hline(margin, y + boxH - 6, W - 2 * margin, LightGrey, framebuffer);
  }
  pushFrame();
}

// ===== Main ===================================================================
static void RunUpdate() {
  if (!connectWiFi(30000)) {
    Serial.println("WiFi failed");
    drawMessage("WiFi connection failed.");
  } else {
    configTzTime(timezone, "0.north-america.pool.ntp.org");
    struct tm t;
    getLocalTime(&t, 10000);
    bool ok = fetchNews();
    Serial.printf("fetch %s, %d items\n", ok ? "ok" : "FAILED", itemCount);
    if (ok) drawNews();
    else drawMessage("Failed to fetch news.");
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  BeginSleep();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);   // USB-CDC: do not block forever without a monitor
  Serial.println("News, starting...");
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) Serial.println("Memory alloc failed!");
  clearFrame();
  RunUpdate();
}

void loop() {   // only reached when BeginSleep() returned, i.e. USB connected
  delay(DevRefreshMinutes * 60UL * 1000UL);
  RunUpdate();
}
