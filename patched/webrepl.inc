// ---------------------------------------------------------------------------
// Web REPL for M5Stack PaperS3
// Serves the IDE page on port 80, evaluates POST /eval bodies as Lisp, and
// provides a small file API over internal flash (/) and the SD card (/sd).
// Requests are only serviced while the serial REPL is idle at its prompt,
// so the uLisp interpreter is never re-entered.
// WiFi: run (wifi-connect "ssid" "password") once; the ESP32 saves the
// credentials and reconnects on every boot.
// ---------------------------------------------------------------------------

#include "webpage.h"

WebServer webserver(80);
String WebOut;
bool WebCapture = false;
bool WebIdle = false;
bool WebStarted = false;

void webcapture (char c) {
  if (WebOut.length() < 64000) WebOut += c;
}

void webeval (const String &src) {
  WebOut = "";
  char *code = stripcomments(src.c_str(), src.length());
  if (!code) { WebOut = "Error: out of memory"; return; }
  RdSrc = code; RdIdx = 0; RdEOF = false;
  object *current_GCStack = GCStack;
  jmp_buf dynamic_handler;
  jmp_buf *previous_handler = handler;
  handler = &dynamic_handler;
  char savedlast = LastChar; LastChar = 0;
  gfun_t savedgfun = GFun;
  char savedprint = LastPrint; LastPrint = '\n';
  WebCapture = true;
  if (!setjmp(dynamic_handler)) {
    evalforms(true);
  } else {
    GCStack = current_GCStack;
    loadcleanup();
  }
  WebCapture = false;
  handler = previous_handler;
  LastChar = savedlast; LastPrint = savedprint;
  GFun = savedgfun;
  clrflag(NOESC); BreakLevel = 0;
  GlobalStringStreamTail = NULL;
  RdSrc = ""; RdIdx = 0;
  free(code);
}

String jsonescape (const char *s) {
  String out;
  for (; *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') {}
    else if (c == '\t') out += "\\t";
    else if ((uint8_t)c < 0x20) {}
    else out += c;
  }
  return out;
}

void webdocs () {
  webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webserver.send(200, "application/json", "[");
  bool firstitem = true;
  for (int n = 0; n < 2; n++) {
    int entries = tablesize(n);
    for (int i = 0; i < entries; i++) {
      const tbl_entry_t *e = &table(n)[i];
      if (e->doc == NULL) continue;
      String item = firstitem ? "" : ",";
      firstitem = false;
      item += "[\"" + jsonescape(e->string) + "\",\"" + jsonescape(e->doc) + "\"," + String(n) + "]";
      webserver.sendContent(item);
    }
  }
  webserver.sendContent("]");
  webserver.sendContent("");
}

void webls () {
  String sub;
  fs::FS *fs = resolvepath(webserver.arg("path").c_str(), sub);
  if (!fs) { webserver.send(404, "text/plain", "filesystem unavailable"); return; }
  File dir = fs->open(sub);
  if (!dir || !dir.isDirectory()) { webserver.send(404, "text/plain", "not a directory"); return; }
  String json = "[";
  File f = dir.openNextFile();
  bool firstitem = true;
  while (f) {
    if (!firstitem) json += ",";
    firstitem = false;
    json += "{\"name\":\"" + jsonescape(f.name()) + "\",\"size\":" + String(f.size()) +
            ",\"dir\":" + (f.isDirectory() ? "true" : "false") + "}";
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  json += "]";
  webserver.send(200, "application/json", json);
}

void webgetfile () {
  String sub;
  fs::FS *fs = resolvepath(webserver.arg("path").c_str(), sub);
  if (!fs) { webserver.send(404, "text/plain", "filesystem unavailable"); return; }
  File f = fs->open(sub, "r");
  if (!f || f.isDirectory()) { webserver.send(404, "text/plain", "not found"); return; }
  webserver.streamFile(f, "text/plain; charset=utf-8");
  f.close();
}

void webputfile () {
  String sub;
  String path = webserver.arg("path");
  fs::FS *fs = resolvepath(path.c_str(), sub);
  if (!fs) { webserver.send(404, "text/plain", "filesystem unavailable"); return; }
  if (sub == "/" || sub.endsWith("/")) { webserver.send(400, "text/plain", "bad file name"); return; }
  File f = fs->open(sub, "w", true);
  if (!f) { webserver.send(500, "text/plain", "can't write file"); return; }
  const String &body = webserver.arg("plain");
  size_t n = f.print(body);
  f.close();
  webserver.send(200, "text/plain", String(n));
}

void webdeletefile () {
  String sub;
  fs::FS *fs = resolvepath(webserver.arg("path").c_str(), sub);
  if (!fs) { webserver.send(404, "text/plain", "filesystem unavailable"); return; }
  File f = fs->open(sub);
  bool isdir = f && f.isDirectory();
  f.close();
  bool ok = isdir ? fs->rmdir(sub) : fs->remove(sub);
  webserver.send(ok ? 200 : 500, "text/plain", ok ? "deleted" : "delete failed");
}

// POST /api/upload?dir=/sd/books (multipart form, any number of files) or ?path=/full/name
File UploadFile;
bool UploadOk = false;
int UploadCount = 0;

void webuploadchunk () {
  HTTPUpload &up = webserver.upload();
  if (up.status == UPLOAD_FILE_START) {
    String path = webserver.hasArg("path") ? webserver.arg("path") : webserver.arg("dir") + "/" + up.filename;
    String sub;
    fs::FS *fs = resolvepath(path.c_str(), sub);
    UploadOk = false;
    if (fs) { UploadFile = fs->open(sub, "w", true); UploadOk = (bool)UploadFile; }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (UploadOk && UploadFile.write(up.buf, up.currentSize) != up.currentSize) UploadOk = false;
  } else if (up.status == UPLOAD_FILE_END) {
    if (UploadFile) UploadFile.close();
    if (UploadOk) UploadCount++;
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (UploadFile) UploadFile.close();
    UploadOk = false;
  }
}

void webuploaddone () {
  webserver.send(UploadOk ? 200 : 500, "text/plain", UploadOk ? String(UploadCount) + " uploaded" : String("upload failed"));
  UploadCount = 0;
}

void webmkdir () {
  String sub;
  fs::FS *fs = resolvepath(webserver.arg("path").c_str(), sub);
  if (!fs) { webserver.send(404, "text/plain", "filesystem unavailable"); return; }
  bool ok = fs->mkdir(sub);
  webserver.send(ok ? 200 : 500, "text/plain", ok ? "created" : "mkdir failed");
}

void webinfo () {
  bool sd = sdmount();
  flashmount();
  String json = "{\"free\":" + String(Freespace + 1) +
    ",\"workspace\":" + String(WORKSPACESIZE) +
    ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
    ",\"rssi\":" + String(WiFi.RSSI()) +
    ",\"battery\":" + String(M5.Power.getBatteryLevel()) +
    ",\"flashTotal\":" + String(FlashMounted ? LittleFS.totalBytes() : 0) +
    ",\"flashUsed\":" + String(FlashMounted ? LittleFS.usedBytes() : 0) +
    ",\"sd\":" + (sd ? "true" : "false") +
    ",\"sdTotal\":" + String(sd ? (double)SD.totalBytes() : 0.0, 0) +
    ",\"sdUsed\":" + String(sd ? (double)SD.usedBytes() : 0.0, 0) + "}";
  webserver.send(200, "application/json", json);
}

// GET /api/screen: the e-ink framebuffer as an 8-bit grayscale BMP
void webscreen () {
  int w = tft.width(), h = tft.height();
  int rowbytes = (w + 3) & ~3;
  uint32_t datasize = rowbytes * h, offset = 14 + 40 + 256 * 4;
  uint8_t hdr[54];
  memset(hdr, 0, sizeof(hdr));
  auto put32 = [&](int at, uint32_t v) { hdr[at] = v; hdr[at+1] = v >> 8; hdr[at+2] = v >> 16; hdr[at+3] = v >> 24; };
  hdr[0] = 'B'; hdr[1] = 'M';
  put32(2, offset + datasize); put32(10, offset);
  put32(14, 40); put32(18, w); put32(22, (uint32_t)(-h));   // negative height: top-down rows
  hdr[26] = 1; hdr[28] = 8; put32(34, datasize); put32(46, 256);
  webserver.setContentLength(offset + datasize);
  webserver.send(200, "image/bmp", "");
  webserver.sendContent((const char *)hdr, sizeof(hdr));
  uint8_t pal[1024];
  for (int i = 0; i < 256; i++) { pal[i*4] = pal[i*4+1] = pal[i*4+2] = i; pal[i*4+3] = 0; }
  webserver.sendContent((const char *)pal, sizeof(pal));
  uint16_t *line = (uint16_t *)malloc(w * 2);
  uint8_t *out = (uint8_t *)malloc(rowbytes);
  if (!line || !out) { free(line); free(out); return; }
  memset(out, 0, rowbytes);
  for (int y = 0; y < h; y++) {
    tft.readRect(0, y, w, 1, line);
    for (int x = 0; x < w; x++) {
      uint16_t c = line[x];
      c = (c >> 8) | (c << 8);                          // readRect returns byte-swapped RGB565
      out[x] = ((c >> 11) * 527 + 23) >> 6;             // red channel, 5 to 8 bits (gray)
    }
    webserver.sendContent((const char *)out, rowbytes);
  }
  free(line); free(out);
}

void webshowstatus (const char *msg) {
  Serial.print("\r\n"); Serial.println(msg);
  #if defined(gfxsupport)
  int cx = tft.getCursorX(), cy = tft.getCursorY();
  float ts = tft.getTextSizeX();
  tft.setTextSize(2);
  tft.fillRect(0, tft.height() - 24, tft.width(), 24, TFT_WHITE);
  tft.setCursor(8, tft.height() - 20);
  tft.print(msg);
  tft.setTextSize(ts);
  tft.setCursor(cx, cy);
  #endif
}

// Runs /autorun.lisp once at boot. Hold a finger on the screen while it starts to skip it.
void autorunfile () {
  static bool done = false;
  if (done) return;
  done = true;
  if (!flashmount() || !LittleFS.exists("/autorun.lisp")) return;
  unsigned long start = millis();
  while (millis() - start < 600) {
    M5.update();
    if (M5.Touch.getDetail().isPressed()) { Serial.println("\r\nAutorun skipped (screen touched)"); return; }
    delay(20);
  }
  Serial.println("\r\nRunning /autorun.lisp (hold a finger on the screen at startup to skip)");
  fn_load(cons(lispstring((char *)"/autorun.lisp"), NULL), NULL);
}

void webinit () {
  flashmount();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();  // uses credentials saved by a previous (wifi-connect ...)
}

// Firmware updates over WiFi: pio run -e ota -t upload
void otastart () {
  ArduinoOTA.setHostname("ulisp");
  ArduinoOTA.setMdnsEnabled(false);       // mDNS is already running; just advertise OTA
  MDNS.enableArduino(3232, false);
  ArduinoOTA.onStart([]() {
    Serial.println("\r\nOTA update starting");
    #if defined(gfxsupport)
    tft.setEpdMode(epd_mode_t::epd_fastest);
    tft.fillRect(0, tft.height() - 24, tft.width(), 24, TFT_WHITE);
    tft.setFont(&fonts::Font0); tft.setTextSize(2);
    tft.setCursor(8, tft.height() - 20);
    tft.print("Updating firmware over WiFi...");
    #endif
  });
  ArduinoOTA.onEnd([]() { Serial.println("OTA update done, restarting"); });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("OTA error %u\r\n", e);
    #if defined(gfxsupport)
    tft.setEpdMode(epd_mode_t::epd_quality);
    tft.fillRect(0, tft.height() - 24, tft.width(), 24, TFT_WHITE);
    #endif
  });
  ArduinoOTA.begin();
}

// Light sleep: switch WiFi off, sleep, then reconnect and restart the web services
int paperlightsleep (uint64_t us, bool touch) {
  bool wifi = WiFi.getMode() != WIFI_OFF;
  if (WebStarted) { webserver.stop(); ArduinoOTA.end(); MDNS.end(); WebStarted = false; }
  if (wifi) { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }
  Serial.flush();
  tft.waitDisplay();
  M5.Power.lightSleep(us, touch);
  int cause = esp_sleep_get_wakeup_cause();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (wifi) { WiFi.mode(WIFI_STA); WiFi.begin(); }   // webpoll restarts the web server once connected
  return cause;
}

void webstart () {
  static bool handlers = false;
  if (handlers) {   // restarting after light sleep
    webserver.begin();
    MDNS.begin("ulisp");
    MDNS.addService("http", "tcp", 80);
    otastart();
    WebStarted = true;
    return;
  }
  handlers = true;
  webserver.on("/", HTTP_GET, []() {
    webserver.send_P(200, "text/html", WEBPAGE);
  });
  webserver.on("/eval", HTTP_POST, []() {
    webeval(webserver.arg("plain"));
    webserver.send(200, "text/plain; charset=utf-8", WebOut);
    WebOut = "";
  });
  webserver.on("/free", HTTP_GET, []() {
    webserver.send(200, "text/plain", String(Freespace + 1));
  });
  webserver.on("/api/info", HTTP_GET, webinfo);
  webserver.on("/api/docs", HTTP_GET, webdocs);
  webserver.on("/api/ls", HTTP_GET, webls);
  webserver.on("/api/file", HTTP_GET, webgetfile);
  webserver.on("/api/file", HTTP_POST, webputfile);
  webserver.on("/api/file", HTTP_DELETE, webdeletefile);
  webserver.on("/api/mkdir", HTTP_POST, webmkdir);
  webserver.on("/api/upload", HTTP_POST, webuploaddone, webuploadchunk);
  webserver.on("/api/screen", HTTP_GET, webscreen);
  webserver.onNotFound([]() { webserver.send(404, "text/plain", "not found"); });
  webserver.begin();
  MDNS.begin("ulisp");
  MDNS.addService("http", "tcp", 80);
  otastart();
  WebStarted = true;
  String msg = "Web REPL: http://" + WiFi.localIP().toString() + "/  (http://ulisp.local/)";
  webshowstatus(msg.c_str());
}

void webpoll () {
  if (!WebIdle) return;
  if (!WebStarted) {
    static unsigned long last = 0;
    if (millis() - last < 500) return;
    last = millis();
    if (WiFi.status() == WL_CONNECTED) webstart();
    return;
  }
  webserver.handleClient();
  ArduinoOTA.handle();
}
