// ---------------------------------------------------------------------------
// PaperS3 display extensions for uLisp (second lookup table)
// ---------------------------------------------------------------------------

const lgfx::IFont *PaperFonts[] = {
  &fonts::Font0,              // 0  built-in 6x8 (default, scaled by set-text-size)
  &fonts::FreeSans9pt7b,      // 1
  &fonts::FreeSans12pt7b,     // 2
  &fonts::FreeSans18pt7b,     // 3
  &fonts::FreeSans24pt7b,     // 4
  &fonts::FreeSansBold12pt7b, // 5
  &fonts::FreeSansBold24pt7b, // 6
  &fonts::FreeSerif12pt7b,    // 7
  &fonts::FreeSerif24pt7b,    // 8
  &fonts::FreeMono12pt7b,     // 9
  &fonts::FreeMono24pt7b,     // 10
  &fonts::DejaVu24,           // 11
  &fonts::DejaVu56,           // 12
  &fonts::Font7,              // 13 7-segment digits
};
const int PaperFontCount = sizeof(PaperFonts) / sizeof(PaperFonts[0]);

uint16_t papercolour (object *arg) { return (uint16_t)checkinteger(arg); }

// (epd-mode [n]) 1 quality, 2 text, 3 fast, 4 fastest
object *fn_epdmode (object *args, object *env) {
  (void) env;
  if (args != NULL) {
    int m = checkinteger(first(args));
    if (m < 1 || m > 4) error("mode must be 1-4", first(args));
    tft.setEpdMode((epd_mode_t)m);
  }
  return number((int)tft.getEpdMode());
}

// (with-batch form*) draw everything, refresh the e-ink once at the end
object *sp_withbatch (object *args, object *env) {
  object *current_GCStack = GCStack;
  jmp_buf dynamic_handler;
  jmp_buf *previous_handler = handler;
  handler = &dynamic_handler;
  object *volatile result = nil;
  object *volatile forms = args;
  bool signaled = false;
  tft.startWrite();
  if (!setjmp(dynamic_handler)) {
    while (forms != NULL) {
      result = eval(car(forms), env);
      if (tstflag(RETURNFLAG)) break;
      forms = cdr(forms);
    }
  } else {
    GCStack = current_GCStack;
    signaled = true;
  }
  handler = previous_handler;
  tft.endWrite();
  if (!signaled) return result;
  GCStack = NULL;
  longjmp(*handler, 1);
}

object *fn_refresh (object *args, object *env) {
  (void) args, (void) env;
  tft.display();
  return nil;
}

object *fn_clearscreen (object *args, object *env) {
  (void) env;
  uint16_t c = TFT_WHITE;
  if (args != NULL) c = papercolour(first(args));
  tft.fillScreen(c);
  tft.setCursor(0, 0);
  return nil;
}

object *fn_gray (object *args, object *env) {
  (void) env;
  int l = checkinteger(first(args));
  if (l < 0) l = 0; if (l > 15) l = 15;
  uint8_t v = l * 17;
  return number(tft.color565(v, v, v));
}

object *fn_setfont (object *args, object *env) {
  (void) env;
  int n = checkinteger(first(args));
  if (n < 0 || n >= PaperFontCount) error("no such font", first(args));
  tft.setFont(PaperFonts[n]);
  return nil;
}

// (draw-text string x y [align]) align 0 left, 1 centre, 2 right; y is the top edge
object *fn_drawtext (object *args, object *env) {
  (void) env;
  object *str = first(args);
  if (!stringp(str)) error(notastring, str);
  int x = checkinteger(second(args)), y = checkinteger(third(args));
  int align = 0;
  if (cddr(cdr(args)) != NULL) align = checkinteger(car(cddr(cdr(args))));
  char buf[256];
  cstring(str, buf, sizeof(buf));
  textdatum_t d = align == 1 ? textdatum_t::top_center : align == 2 ? textdatum_t::top_right : textdatum_t::top_left;
  textdatum_t old = tft.getTextDatum();
  tft.setTextDatum(d);
  int w = tft.drawString(buf, x, y);
  tft.setTextDatum(old);
  return number(w);
}

object *fn_textwidth (object *args, object *env) {
  (void) env;
  object *str = first(args);
  if (!stringp(str)) error(notastring, str);
  char buf[256];
  cstring(str, buf, sizeof(buf));
  return number(tft.textWidth(buf));
}

object *fn_fontheight (object *args, object *env) {
  (void) args, (void) env;
  return number(tft.fontHeight());
}

object *fn_drawellipse (object *args, object *env) {
  (void) env;
  int x = checkinteger(first(args)), y = checkinteger(second(args));
  int rx = checkinteger(third(args)), ry = checkinteger(car(cdr(cddr(args))));
  uint16_t c = TFT_BLACK; if (cddr(cddr(args)) != NULL) c = papercolour(car(cddr(cddr(args))));
  tft.drawEllipse(x, y, rx, ry, c);
  return nil;
}

object *fn_fillellipse (object *args, object *env) {
  (void) env;
  int x = checkinteger(first(args)), y = checkinteger(second(args));
  int rx = checkinteger(third(args)), ry = checkinteger(car(cdr(cddr(args))));
  uint16_t c = TFT_BLACK; if (cddr(cddr(args)) != NULL) c = papercolour(car(cddr(cddr(args))));
  tft.fillEllipse(x, y, rx, ry, c);
  return nil;
}

// (fill-arc x y r0 r1 angle0 angle1 [colour]) angles in degrees, 0 = 3 o'clock, clockwise
object *fn_fillarc (object *args, object *env) {
  (void) env;
  int v[6];
  for (int i = 0; i < 6; i++) { v[i] = checkinteger(first(args)); args = cdr(args); }
  uint16_t c = TFT_BLACK; if (args != NULL) c = papercolour(first(args));
  tft.fillArc(v[0], v[1], v[2], v[3], (float)v[4], (float)v[5], c);
  return nil;
}

// (draw-qrcode string x y size)
object *fn_drawqrcode (object *args, object *env) {
  (void) env;
  object *str = first(args);
  if (!stringp(str)) error(notastring, str);
  char buf[256];
  cstring(str, buf, sizeof(buf));
  tft.qrcode(buf, checkinteger(second(args)), checkinteger(third(args)), checkinteger(car(cdr(cddr(args)))));
  return nil;
}

// (wait-touch [timeout-ms]) waits for a new tap, returns (x y) or nil on timeout
object *fn_waittouch (object *args, object *env) {
  (void) env;
  unsigned long timeout = 0, start = millis();
  if (args != NULL) timeout = checkinteger(first(args));
  for (;;) {
    M5.update();
    touch = M5.Touch.getDetail();
    if (touch.wasPressed()) return cons(number(touch.x), cons(number(touch.y), NULL));
    if (timeout && millis() - start >= timeout) return nil;
    testescape();
    delay(10);
  }
}

object *fn_battery (object *args, object *env) {
  (void) args, (void) env;
  return number(M5.Power.getBatteryLevel());
}

// ---- Helpers -----------------------------------------------------------------

extern gfun_t GFun;  // defined later, with the reader

// Builds a Lisp string from raw bytes (no escape processing)
object *rawstring (const char *s, size_t len) {
  object *obj = newstring();
  object *tail = obj;
  for (size_t i = 0; i < len; i++) buildstring(s[i], &tail);
  return obj;
}

String StrAcc;
void stracc (char c) { StrAcc += c; }

// Copies a Lisp string into StrAcc
void lisptostracc (object *str) {
  if (!stringp(str)) error(notastring, str);
  StrAcc = "";
  flags_t saved = Flags;
  clrflag(PRINTREADABLY);  // copy raw characters, without escaping quotes and backslashes
  plispstr(str->name, stracc);
  Flags = saved;
}

object *makelist3f (float a, float b, float c) {
  return cons(makefloat(a), cons(makefloat(b), cons(makefloat(c), NULL)));
}

// ---- Shared string reader (used by load and the web REPL) ------------------

const char *RdSrc = "";
int RdIdx = 0;
bool RdEOF = false;

int grd () {
  char c = RdSrc[RdIdx];
  if (c == 0) {
    if (RdEOF) error2("incomplete expression");
    RdEOF = true;
    return -1;
  }
  RdIdx++;
  return c;
}

bool rdmore () {
  char c;
  while ((c = RdSrc[RdIdx]) == ' ' || c == '\n' || c == '\r' || c == '\t') RdIdx++;
  return RdSrc[RdIdx] != 0;
}

// Returns a malloc'd copy of src with ; comments removed and a trailing newline
char *stripcomments (const char *src, size_t len) {
  char *out = (char *)heap_caps_malloc(len + 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!out) return NULL;
  size_t o = 0; bool instring = false;
  for (size_t i = 0; i < len; i++) {
    char c = src[i];
    if (c == 0) break;
    if (instring) {
      out[o++] = c;
      if (c == '\\' && i + 1 < len) out[o++] = src[++i];
      else if (c == '"') instring = false;
    } else if (c == '"') { instring = true; out[o++] = c; }
    else if (c == '#' && i + 2 < len && src[i+1] == '\\') { out[o++] = c; out[o++] = src[++i]; out[o++] = src[++i]; }
    else if (c == ';') { while (i < len && src[i] != '\n') i++; out[o++] = '\n'; }
    else out[o++] = c;
  }
  out[o++] = '\n'; out[o] = 0;
  return out;
}

// Reads and evaluates every form in RdSrc; optionally prints each result
object *evalforms (bool printresults) {
  object *result = nil;
  while (rdmore()) {
    Context = NIL;
    object *line = readmain(grd);
    if (line == (object *)KET) error2("unmatched right bracket");
    protect(line);
    result = eval(line, NULL);
    unprotect();
    if (printresults) { pfl(pserial); printobject(result, pserial); pln(pserial); }
  }
  return result;
}

// Buffers owned by in-progress loads; freed by loadcleanup() after an error
#define MAXLOADDEPTH 8
char *LoadBufs[MAXLOADDEPTH];
int LoadDepth = 0;

void loadcleanup () {
  for (int i = 0; i < LoadDepth; i++) { free(LoadBufs[i]); LoadBufs[i] = NULL; }
  LoadDepth = 0;
}

// ---- Filesystems: "/sd/..." is the SD card, anything else is internal flash --

SPIClass SDSPI(HSPI);
bool SdMounted = false;
bool FlashMounted = false;

bool flashmount () {
  if (!FlashMounted) FlashMounted = LittleFS.begin(true);
  return FlashMounted;
}

bool sdmount () {
  if (SdMounted) return true;
  static bool spistarted = false;
  if (!spistarted) { SDSPI.begin(39, 40, 38, 47); spistarted = true; }
  SdMounted = SD.begin(47, SDSPI, 25000000, "/sdcard");
  return SdMounted;
}

// Resolves path to a filesystem and the path within it; NULL if unavailable
fs::FS *resolvepath (const char *path, String &sub) {
  String p = path;
  if (!p.startsWith("/")) p = "/" + p;
  if (p == "/sd" || p.startsWith("/sd/")) {
    if (!sdmount()) return NULL;
    sub = p.length() > 3 ? p.substring(3) : String("/");
    return &SD;
  }
  if (!flashmount()) return NULL;
  sub = p;
  return &LittleFS;
}

fs::FS *lisppath (object *arg, String &sub) {
  if (!stringp(arg)) error(notastring, arg);
  char buf[160];
  cstring(arg, buf, sizeof(buf));
  fs::FS *fs = resolvepath(buf, sub);
  if (!fs) error(String(buf).startsWith("/sd") ? "no SD card" : "flash filesystem unavailable", arg);
  return fs;
}

// Reads a whole file into a malloc'd, comment-stripped buffer
char *readfilebuf (fs::FS *fs, const String &sub, size_t *lenout, bool strip) {
  File f = fs->open(sub, "r");
  if (!f || f.isDirectory()) return NULL;
  size_t len = f.size();
  char *raw = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!raw) { f.close(); return NULL; }
  len = f.read((uint8_t *)raw, len);
  raw[len] = 0;
  f.close();
  if (lenout) *lenout = len;
  if (!strip) return raw;
  char *out = stripcomments(raw, len);
  free(raw);
  return out;
}

object *fn_listfiles (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = args ? lisppath(first(args), sub) : resolvepath("/", sub);
  if (!fs) error2("flash filesystem unavailable");
  File dir = fs->open(sub);
  if (!dir || !dir.isDirectory()) error2("not a directory");
  object *head = cons(nil, NULL), *tail = head;
  protect(head);
  File f = dir.openNextFile();
  while (f) {
    String name = f.name();
    if (f.isDirectory()) name += "/";
    f.close();
    cdr(tail) = cons(rawstring(name.c_str(), name.length()), NULL);
    tail = cdr(tail);
    f = dir.openNextFile();
  }
  dir.close();
  unprotect();
  return cdr(head);
}

object *fn_readfile (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = lisppath(first(args), sub);
  size_t len = 0;
  char *buf = readfilebuf(fs, sub, &len, false);
  if (!buf) error("can't read file", first(args));
  object *result = rawstring(buf, len);
  free(buf);
  return result;
}

// (write-file path string [append])
object *fn_writefile (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = lisppath(first(args), sub);
  lisptostracc(second(args));
  bool append = cddr(args) != NULL && third(args) != nil;
  File f = fs->open(sub, append ? "a" : "w", true);
  if (!f) error("can't write file", first(args));
  size_t n = f.print(StrAcc);
  f.close();
  StrAcc = "";
  return number(n);
}

object *fn_deletefile (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = lisppath(first(args), sub);
  File f = fs->open(sub);
  bool isdir = f && f.isDirectory();
  f.close();
  return (isdir ? fs->rmdir(sub) : fs->remove(sub)) ? tee : nil;
}

object *fn_makedirectory (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = lisppath(first(args), sub);
  return fs->mkdir(sub) ? tee : nil;
}

object *fn_fileexists (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = lisppath(first(args), sub);
  return fs->exists(sub) ? tee : nil;
}

object *fn_sdcardp (object *args, object *env) {
  (void) args, (void) env;
  return sdmount() ? tee : nil;
}

// (load path) evaluates every form in a file
object *fn_load (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = lisppath(first(args), sub);
  if (LoadDepth >= MAXLOADDEPTH) error2("loads nested too deeply");
  char *buf = readfilebuf(fs, sub, NULL, true);
  if (!buf) error("can't read file", first(args));
  LoadBufs[LoadDepth++] = buf;
  const char *savedsrc = RdSrc; int savedidx = RdIdx; bool savedeof = RdEOF;
  gfun_t savedgfun = GFun; char savedlast = LastChar;
  RdSrc = buf; RdIdx = 0; RdEOF = false;
  evalforms(false);
  RdSrc = savedsrc; RdIdx = savedidx; RdEOF = savedeof;
  GFun = savedgfun; LastChar = savedlast;
  free(LoadBufs[--LoadDepth]); LoadBufs[LoadDepth] = NULL;
  return tee;
}

// ---- IMU (BMI270) --------------------------------------------------------------

object *fn_accel (object *args, object *env) {
  (void) args, (void) env;
  float x, y, z;
  if (!M5.Imu.isEnabled() || !M5.Imu.getAccel(&x, &y, &z)) error2("IMU not available");
  return makelist3f(x, y, z);
}

object *fn_gyro (object *args, object *env) {
  (void) args, (void) env;
  float x, y, z;
  if (!M5.Imu.isEnabled() || !M5.Imu.getGyro(&x, &y, &z)) error2("IMU not available");
  return makelist3f(x, y, z);
}

// ---- Buzzer --------------------------------------------------------------------

// (tone freq [ms]) plays a tone; without ms it plays until (tone 0)
object *fn_tone (object *args, object *env) {
  (void) env;
  int freq = checkinteger(first(args));
  if (!M5.Speaker.isEnabled()) M5.Speaker.begin();
  if (freq <= 0) { M5.Speaker.stop(); return nil; }
  uint32_t ms = UINT32_MAX;
  if (cdr(args) != NULL) ms = checkinteger(second(args));
  M5.Speaker.tone(freq, ms);
  return nil;
}

// (beep [freq ms]) blocking beep, default 2000 Hz for 100 ms
object *fn_beep (object *args, object *env) {
  (void) env;
  int freq = 2000, ms = 100;
  if (args != NULL) { freq = checkinteger(first(args)); if (cdr(args) != NULL) ms = checkinteger(second(args)); }
  if (!M5.Speaker.isEnabled()) M5.Speaker.begin();
  M5.Speaker.tone(freq, ms);
  delay(ms);
  return nil;
}

object *fn_volume (object *args, object *env) {
  (void) env;
  if (args != NULL) {
    int v = checkinteger(first(args));
    if (v < 0) v = 0; if (v > 255) v = 255;
    M5.Speaker.setVolume(v);
  }
  return number(M5.Speaker.getVolume());
}

// ---- RTC -----------------------------------------------------------------------

object *fn_rtctime (object *args, object *env) {
  (void) args, (void) env;
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.isEnabled() || !M5.Rtc.getDateTime(&dt)) error2("RTC not available");
  return cons(number(dt.date.year), cons(number(dt.date.month), cons(number(dt.date.date),
    cons(number(dt.time.hours), cons(number(dt.time.minutes), cons(number(dt.time.seconds), NULL))))));
}

// (set-rtc year month day hour minute second), or (set-rtc) to copy the NTP clock
object *fn_setrtc (object *args, object *env) {
  (void) env;
  if (!M5.Rtc.isEnabled()) error2("RTC not available");
  m5::rtc_datetime_t dt;
  if (args == NULL) {
    time_t now; time(&now);
    if (now < 31536000) error2("no network time yet; connect to WiFi first");
    now += TZ_OFFSET;
    struct tm t; gmtime_r(&now, &t);
    dt.date.year = t.tm_year + 1900; dt.date.month = t.tm_mon + 1; dt.date.date = t.tm_mday;
    dt.date.weekDay = t.tm_wday;
    dt.time.hours = t.tm_hour; dt.time.minutes = t.tm_min; dt.time.seconds = t.tm_sec;
  } else {
    int v[6];
    for (int i = 0; i < 6; i++) {
      if (args == NULL) error2(toofewargs);
      v[i] = checkinteger(first(args)); args = cdr(args);
    }
    dt.date.year = v[0]; dt.date.month = v[1]; dt.date.date = v[2]; dt.date.weekDay = 0;
    dt.time.hours = v[3]; dt.time.minutes = v[4]; dt.time.seconds = v[5];
  }
  M5.Rtc.setDateTime(dt);
  return fn_rtctime(NULL, NULL);
}

// ---- Network -------------------------------------------------------------------

int LastHttpStatus = 0;
object *fn_wifiwait (object *args, object *env);   // defined with the power functions

// Performs a request; returns true with the body in out, or false with a message in out
bool httprequest (const char *url, const char *method, const String *body, const char *ctype, String &out) {
  HTTPClient http;
  WiFiClient plain;
  WiFiClientSecure secure;
  bool ok;
  if (strncmp(url, "https:", 6) == 0) { secure.setInsecure(); ok = http.begin(secure, url); }
  else ok = http.begin(plain, url);
  if (!ok) { out = "invalid URL"; return false; }
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.setUserAgent("uLisp-PaperS3");
  int code;
  if (body) {
    http.addHeader("Content-Type", ctype ? ctype : "text/plain");
    code = http.sendRequest(method, *body);
  } else code = http.GET();
  LastHttpStatus = code;
  if (code < 0) { out = HTTPClient::errorToString(code); http.end(); return false; }
  int size = http.getSize();
  if (size > 256000) { out = "response too large"; http.end(); return false; }
  out = http.getString();
  http.end();
  return true;
}

object *httpresult (bool ok, String &out) {
  if (!ok) { String msg = out; out = ""; error2(msg.c_str()); }
  object *result = rawstring(out.c_str(), out.length());
  out = "";
  return result;
}

object *fn_httpget (object *args, object *env) {
  (void) env;
  if (fn_wifiwait(NULL, NULL) == nil) error2("not connected to WiFi");
  char url[256];
  if (!stringp(first(args))) error(notastring, first(args));
  cstring(first(args), url, sizeof(url));
  String out;
  bool ok = httprequest(url, "GET", NULL, NULL, out);
  return httpresult(ok, out);
}

// (http-post url body [content-type])
object *fn_httppost (object *args, object *env) {
  (void) env;
  if (fn_wifiwait(NULL, NULL) == nil) error2("not connected to WiFi");
  char url[256], ctype[64];
  if (!stringp(first(args))) error(notastring, first(args));
  cstring(first(args), url, sizeof(url));
  const char *ct = NULL;
  if (cddr(args) != NULL) { cstring(third(args), ctype, sizeof(ctype)); ct = ctype; }
  lisptostracc(second(args));
  String body = StrAcc; StrAcc = "";
  String out;
  bool ok = httprequest(url, "POST", &body, ct, out);
  body = "";
  return httpresult(ok, out);
}

object *fn_httpstatus (object *args, object *env) {
  (void) args, (void) env;
  return number(LastHttpStatus);
}

object *fn_wifirssi (object *args, object *env) {
  (void) args, (void) env;
  if (WiFi.status() != WL_CONNECTED) return nil;
  return number(WiFi.RSSI());
}

// ---- JSON ------------------------------------------------------------------------
// Objects become association lists with string keys, arrays become lists,
// true -> t, false and null -> nil. Allocation in C never triggers GC
// (uLisp only collects inside eval), so partial results need no protection.

#define JSONMAXDEPTH 64

const char *JsonSrc = "";
size_t JsonLen = 0, JsonPos = 0;
String JsonBuf;   // holds input or output text outside Lisp memory

void jsonerror (const char *msg) {
  char buf[80];
  snprintf(buf, sizeof(buf), "JSON %s at offset %u", msg, (unsigned)JsonPos);
  JsonBuf = "";
  error2(buf);
}

void jsonws () {
  while (JsonPos < JsonLen) {
    char c = JsonSrc[JsonPos];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') JsonPos++; else break;
  }
}

char jsonpeek () { return JsonPos < JsonLen ? JsonSrc[JsonPos] : 0; }

void jsonexpect (char c, const char *msg) {
  jsonws();
  if (jsonpeek() != c) jsonerror(msg);
  JsonPos++;
}

int jsonhex4 () {
  if (JsonPos + 4 > JsonLen) jsonerror("bad \\u escape");
  int v = 0;
  for (int i = 0; i < 4; i++) {
    char c = JsonSrc[JsonPos++];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= c - '0';
    else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    else jsonerror("bad \\u escape");
  }
  return v;
}

void jsonutf8 (uint32_t cp, object **tail) {
  if (cp == 0) return;  // uLisp strings can't hold NUL
  if (cp < 0x80) buildstring(cp, tail);
  else if (cp < 0x800) { buildstring(0xC0 | (cp >> 6), tail); buildstring(0x80 | (cp & 0x3F), tail); }
  else if (cp < 0x10000) {
    buildstring(0xE0 | (cp >> 12), tail); buildstring(0x80 | ((cp >> 6) & 0x3F), tail);
    buildstring(0x80 | (cp & 0x3F), tail);
  } else {
    buildstring(0xF0 | (cp >> 18), tail); buildstring(0x80 | ((cp >> 12) & 0x3F), tail);
    buildstring(0x80 | ((cp >> 6) & 0x3F), tail); buildstring(0x80 | (cp & 0x3F), tail);
  }
}

// Called with JsonPos just after the opening quote
object *jsonstring () {
  object *obj = newstring();
  object *tail = obj;
  for (;;) {
    if (JsonPos >= JsonLen) jsonerror("unterminated string");
    char c = JsonSrc[JsonPos++];
    if (c == '"') return obj;
    if (c != '\\') { if (c != 0) buildstring(c, &tail); continue; }
    if (JsonPos >= JsonLen) jsonerror("unterminated string");
    char e = JsonSrc[JsonPos++];
    switch (e) {
      case '"': case '\\': case '/': buildstring(e, &tail); break;
      case 'n': buildstring('\n', &tail); break;
      case 't': buildstring('\t', &tail); break;
      case 'r': buildstring('\r', &tail); break;
      case 'b': buildstring('\b', &tail); break;
      case 'f': buildstring('\f', &tail); break;
      case 'u': {
        uint32_t cp = jsonhex4();
        if (cp >= 0xD800 && cp <= 0xDBFF && JsonPos + 6 <= JsonLen &&
            JsonSrc[JsonPos] == '\\' && JsonSrc[JsonPos+1] == 'u') {
          JsonPos += 2;
          uint32_t lo = jsonhex4();
          if (lo >= 0xDC00 && lo <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          else { jsonutf8(0xFFFD, &tail); cp = lo; }
        }
        jsonutf8(cp, &tail);
        break;
      }
      default: jsonerror("bad escape");
    }
  }
}

object *jsonnumber () {
  size_t start = JsonPos;
  bool isfloat = false;
  if (jsonpeek() == '-') JsonPos++;
  if (!isdigit((unsigned char)jsonpeek())) jsonerror("bad number");
  while (JsonPos < JsonLen) {
    char c = JsonSrc[JsonPos];
    if (isdigit((unsigned char)c)) JsonPos++;
    else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') { isfloat = true; JsonPos++; }
    else break;
  }
  char buf[40];
  size_t n = JsonPos - start;
  if (n >= sizeof(buf)) jsonerror("number too long");
  memcpy(buf, JsonSrc + start, n); buf[n] = 0;
  char *end;
  if (!isfloat) {
    errno = 0;
    long long v = strtoll(buf, &end, 10);
    if (*end == 0 && errno == 0 && v >= INT32_MIN && v <= INT32_MAX) return number((int)v);
  }
  double d = strtod(buf, &end);
  if (*end != 0) jsonerror("bad number");
  return makefloat((float)d);
}

object *jsonvalue (int depth) {
  if (depth > JSONMAXDEPTH) jsonerror("nested too deeply");
  jsonws();
  char c = jsonpeek();
  if (c == '{') {
    JsonPos++;
    object *head = cons(nil, NULL), *tail = head;
    jsonws();
    if (jsonpeek() == '}') { JsonPos++; return nil; }
    for (;;) {
      jsonexpect('"', "expected string key");
      object *pair = cons(nil, nil);
      cdr(tail) = cons(pair, NULL); tail = cdr(tail);
      car(pair) = jsonstring();
      jsonexpect(':', "expected ':'");
      cdr(pair) = jsonvalue(depth + 1);
      jsonws();
      char d = jsonpeek();
      if (d == ',') { JsonPos++; continue; }
      if (d == '}') { JsonPos++; return cdr(head); }
      jsonerror("expected ',' or '}'");
    }
  }
  if (c == '[') {
    JsonPos++;
    object *head = cons(nil, NULL), *tail = head;
    jsonws();
    if (jsonpeek() == ']') { JsonPos++; return nil; }
    for (;;) {
      cdr(tail) = cons(nil, NULL); tail = cdr(tail);
      car(tail) = jsonvalue(depth + 1);
      jsonws();
      char d = jsonpeek();
      if (d == ',') { JsonPos++; continue; }
      if (d == ']') { JsonPos++; return cdr(head); }
      jsonerror("expected ',' or ']'");
    }
  }
  if (c == '"') { JsonPos++; return jsonstring(); }
  if (c == '-' || isdigit((unsigned char)c)) return jsonnumber();
  if (JsonLen - JsonPos >= 4 && strncmp(JsonSrc + JsonPos, "true", 4) == 0) { JsonPos += 4; return tee; }
  if (JsonLen - JsonPos >= 5 && strncmp(JsonSrc + JsonPos, "false", 5) == 0) { JsonPos += 5; return nil; }
  if (JsonLen - JsonPos >= 4 && strncmp(JsonSrc + JsonPos, "null", 4) == 0) { JsonPos += 4; return nil; }
  jsonerror(c ? "unexpected character" : "unexpected end");
  return nil;
}

// Parses the text in JsonBuf, then frees it
object *jsonparsebuf () {
  JsonSrc = JsonBuf.c_str(); JsonLen = JsonBuf.length(); JsonPos = 0;
  object *result = jsonvalue(0);
  jsonws();
  if (JsonPos < JsonLen) jsonerror("unexpected data after value");
  JsonBuf = ""; JsonSrc = ""; JsonLen = 0;
  return result;
}

// Lisp string or symbol name -> Arduino String (raw characters)
String KeyAcc;
void keyacc (char c) { KeyAcc += c; }
String lispname (object *obj) {
  KeyAcc = "";
  flags_t saved = Flags;
  clrflag(PRINTREADABLY);
  if (stringp(obj)) plispstr(obj->name, keyacc);
  else if (symbolp(obj)) psymbol(obj->name, keyacc);
  Flags = saved;
  String s = KeyAcc; KeyAcc = "";
  return s;
}

void jsonquote (const String &s) {
  JsonBuf += '"';
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"': JsonBuf += "\\\""; break;
      case '\\': JsonBuf += "\\\\"; break;
      case '\n': JsonBuf += "\\n"; break;
      case '\r': JsonBuf += "\\r"; break;
      case '\t': JsonBuf += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); JsonBuf += b; }
        else JsonBuf += c;
    }
  }
  JsonBuf += '"';
}

bool jsonobjectp (object *obj) {
  for (; consp(obj); obj = cdr(obj)) {
    object *e = car(obj);
    if (!consp(e) || !(stringp(car(e)) || symbolp(car(e)))) return false;
  }
  return true;
}

void jsonnewline (bool pretty, int depth) {
  if (!pretty) return;
  JsonBuf += '\n';
  for (int i = 0; i < depth; i++) JsonBuf += "  ";
}

void jsonwrite (object *obj, bool pretty, int depth) {
  if (depth > JSONMAXDEPTH) { JsonBuf = ""; error2("data nested too deeply for JSON"); }
  if (obj == nil) { JsonBuf += "null"; return; }
  if (symbolp(obj) && obj->name == sym(TEE)) { JsonBuf += "true"; return; }
  if (integerp(obj)) { JsonBuf += String(obj->integer); return; }
  if (floatp(obj)) {
    float f = obj->single_float;
    if (isnan(f) || isinf(f)) { JsonBuf += "null"; return; }
    char b[24]; snprintf(b, sizeof(b), "%.7g", f); JsonBuf += b; return;
  }
  if (stringp(obj) || symbolp(obj)) { jsonquote(lispname(obj)); return; }
  if (characterp(obj)) { String s; s += (char)obj->chars; jsonquote(s); return; }
  if (consp(obj)) {
    bool isobj = jsonobjectp(obj);
    JsonBuf += isobj ? '{' : '[';
    bool firstitem = true;
    for (; consp(obj); obj = cdr(obj)) {
      if (!firstitem) JsonBuf += ',';
      firstitem = false;
      jsonnewline(pretty, depth + 1);
      if (isobj) {
        jsonquote(lispname(car(car(obj))));
        JsonBuf += pretty ? ": " : ":";
        jsonwrite(cdr(car(obj)), pretty, depth + 1);
      } else jsonwrite(car(obj), pretty, depth + 1);
    }
    jsonnewline(pretty, depth);
    JsonBuf += isobj ? '}' : ']';
    return;
  }
  JsonBuf = ""; error2("value can't be converted to JSON");
}

object *fn_jsonparse (object *args, object *env) {
  (void) env;
  lisptostracc(first(args));
  JsonBuf = StrAcc; StrAcc = "";
  return jsonparsebuf();
}

// (json-string data [pretty])
object *fn_jsonstring (object *args, object *env) {
  (void) env;
  bool pretty = cdr(args) != NULL && second(args) != nil;
  JsonBuf = "";
  jsonwrite(first(args), pretty, 0);
  object *result = rawstring(JsonBuf.c_str(), JsonBuf.length());
  JsonBuf = "";
  return result;
}

// (json-get data key-or-index ...)
object *fn_jsonget (object *args, object *env) {
  (void) env;
  object *data = first(args);
  for (object *keys = cdr(args); keys != NULL; keys = cdr(keys)) {
    object *k = car(keys);
    if (integerp(k)) {
      int n = k->integer;
      if (n < 0) return nil;
      while (n-- > 0 && consp(data)) data = cdr(data);
      if (!consp(data)) return nil;
      data = car(data);
    } else if (stringp(k) || symbolp(k)) {
      String want = lispname(k);
      object *found = NULL;
      for (object *p = data; consp(p); p = cdr(p)) {
        object *e = car(p);
        if (consp(e) && (stringp(car(e)) || symbolp(car(e))) && lispname(car(e)) == want) { found = e; break; }
      }
      if (!found) return nil;
      data = cdr(found);
    } else error("key must be a string or an index", k);
  }
  return data;
}

object *fn_httpgetjson (object *args, object *env) {
  (void) env;
  if (fn_wifiwait(NULL, NULL) == nil) error2("not connected to WiFi");
  char url[256];
  if (!stringp(first(args))) error(notastring, first(args));
  cstring(first(args), url, sizeof(url));
  JsonBuf = "";
  bool ok = httprequest(url, "GET", NULL, NULL, JsonBuf);
  if (!ok) { String msg = JsonBuf; JsonBuf = ""; error2(msg.c_str()); }
  if (LastHttpStatus >= 400) {
    char msg[48]; snprintf(msg, sizeof(msg), "HTTP status %d", LastHttpStatus);
    JsonBuf = ""; error2(msg);
  }
  return jsonparsebuf();
}

const char doc_jsonparse[] = "(json-parse string)\n"
"Parses JSON. Objects become association lists with string keys, arrays become lists,\n"
"true becomes t, and false and null become nil.";
const char doc_jsonstring[] = "(json-string data [pretty])\n"
"Converts Lisp data to a JSON string. A list of (key . value) pairs becomes an object,\n"
"any other list an array, nil null, and t true. With pretty true, the output is indented.";
const char doc_jsonget[] = "(json-get data key-or-index*)\n"
"Follows a path into parsed JSON, e.g. (json-get data \"items\" 0 \"name\"). Returns nil if the path is missing.";
const char doc_httpgetjson[] = "(http-get-json url)\n"
"Fetches url and parses the response as JSON, using less memory than (json-parse (http-get url)).";

#include "reader.inc"

// ---- Power: sleep modes, wake reason, autorun --------------------------------------

const char *WakeReason = "power-on";   // set once at boot by initwake()

void initwake () {
  esp_reset_reason_t r = esp_reset_reason();
  bool alarm = M5.Rtc.isEnabled() && M5.Rtc.getIRQstatus();
  if (M5.Rtc.isEnabled()) { M5.Rtc.disableIRQ(); M5.Rtc.clearIRQ(); }
  if (alarm) WakeReason = "alarm";
  else if (r == ESP_RST_DEEPSLEEP) WakeReason = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER ? "timer" : "deep-sleep";
  else if (r == ESP_RST_SW) WakeReason = "restart";
  else if (r == ESP_RST_EXT) WakeReason = "reset";
  else if (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT) WakeReason = "crash";
  else if (r == ESP_RST_BROWNOUT) WakeReason = "brownout";
  else WakeReason = "power-on";
}

object *fn_wakereason (object *args, object *env) {
  (void) args, (void) env;
  return internlong((char *)WakeReason);
}

uint64_t sleepmicros (object *arg) {
  if (arg == nil) return M5.Power.sleep_no_timer;
  float secs = integerp(arg) ? (float)arg->integer : checkintfloat(arg);
  if (secs <= 0) error("seconds must be positive", arg);
  return (uint64_t)(secs * 1000000.0f);
}

// (light-sleep [secs] [touch]) sleeps, keeping the Lisp state; returns timer or touch
object *fn_lightsleep (object *args, object *env) {
  (void) env;
  uint64_t us = args ? sleepmicros(first(args)) : M5.Power.sleep_no_timer;
  bool touch = !(args && cdr(args) && second(args) == nil);
  if (us == M5.Power.sleep_no_timer && !touch) error2("nothing would wake the board: give seconds or allow touch");
  int cause = paperlightsleep(us, touch);
  return internlong((char *)(cause == ESP_SLEEP_WAKEUP_TIMER ? "timer" : cause == ESP_SLEEP_WAKEUP_GPIO ? "touch" : "other"));
}

// (deep-sleep [secs]) sleeps and restarts; without secs only reset or the power button wake it
object *fn_deepsleep (object *args, object *env) {
  (void) env;
  uint64_t us = args ? sleepmicros(first(args)) : M5.Power.sleep_no_timer;
  Serial.flush();
  tft.waitDisplay();
  M5.Power.deepSleep(us, false);
  return nil;
}

// (power-off [secs]) switches the board off; with secs the clock alarm turns it back on
object *fn_poweroff (object *args, object *env) {
  (void) env;
  int secs = 0;
  if (args && first(args) != nil) {
    secs = checkinteger(first(args));
    if (secs < 1 || secs > 255 * 60) error("seconds must be 1 to 15300 (255 minutes)", first(args));
  }
  Serial.flush();
  tft.waitDisplay();
  if (secs) M5.Power.timerSleep(secs); else M5.Power.powerOff();
  error2("the board could not switch off");
  return nil;
}

// Waits up to secs (default 15) for WiFi; returns t when connected
object *fn_wifiwait (object *args, object *env) {
  (void) env;
  int secs = args ? checkinteger(first(args)) : 15;
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < (unsigned long)secs * 1000UL) { testescape(); delay(100); }
  return WiFi.status() == WL_CONNECTED ? tee : nil;
}

const char doc_wakereason[] = "(wake-reason)\n"
"Why the board last started: power-on, alarm (clock alarm after power-off), timer (deep-sleep timer),\n"
"restart, reset, crash or brownout.";
const char doc_lightsleep[] = "(light-sleep [secs] [touch])\n"
"Sleeps with the Lisp state kept, until secs pass or the screen is touched (unless touch is nil).\n"
"WiFi is switched off while sleeping and reconnects afterwards. Returns timer or touch.";
const char doc_deepsleep[] = "(deep-sleep [secs])\n"
"Sleeps for secs, then restarts (memory is lost; see /autorun.lisp). Without secs it sleeps until reset.";
const char doc_poweroff[] = "(power-off [secs])\n"
"Switches the board off; the screen keeps its image. With secs (up to 15300) the clock alarm switches it\n"
"back on. On USB power it can't switch off, so it deep-sleeps instead.";
const char doc_wifiwait[] = "(wifi-wait [secs])\n"
"Waits up to secs (default 15) for WiFi to connect. Returns t if connected.";

const char doc_listfiles[] = "(list-files [dir])\n"
"Lists a directory as strings; subdirectories end in /. \"/\" is internal flash, \"/sd\" the SD card.";
const char doc_readfile[] = "(read-file path)\n"
"Returns the contents of a file as a string. Paths starting /sd/ are on the SD card.";
const char doc_writefile[] = "(write-file path string [append])\n"
"Writes string to a file, replacing it unless append is true. Returns the bytes written.";
const char doc_deletefile[] = "(delete-file path)\n"
"Deletes a file or an empty directory.";
const char doc_makedirectory[] = "(make-directory path)\n"
"Creates a directory.";
const char doc_fileexists[] = "(file-exists path)\n"
"Returns t if the file or directory exists.";
const char doc_sdcardp[] = "(sd-card-p)\n"
"Returns t if an SD card is inserted and mounted.";
const char doc_load[] = "(load path)\n"
"Reads and evaluates every form in a Lisp file.";
const char doc_accel[] = "(accel)\n"
"Returns the accelerometer reading (x y z) in g.";
const char doc_gyro[] = "(gyro)\n"
"Returns the gyroscope reading (x y z) in degrees per second.";
const char doc_tone[] = "(tone freq [ms])\n"
"Plays a tone on the buzzer without waiting. Without ms it plays until (tone 0).";
const char doc_beep[] = "(beep [freq ms])\n"
"Beeps and waits until it finishes. Defaults to 2000 Hz for 100 ms.";
const char doc_volume[] = "(volume [0-255])\n"
"Sets the buzzer volume. Returns the current volume.";
const char doc_rtctime[] = "(rtc-time)\n"
"Returns the battery-backed clock as (year month day hour minute second).";
const char doc_setrtc[] = "(set-rtc [year month day hour minute second])\n"
"Sets the battery-backed clock. With no arguments, copies the network (NTP) time.";
const char doc_httpget[] = "(http-get url)\n"
"Fetches url (http or https) and returns the body as a string. See http-status.";
const char doc_httppost[] = "(http-post url body [content-type])\n"
"Posts body to url and returns the response body as a string.";
const char doc_httpstatus[] = "(http-status)\n"
"Returns the HTTP status code of the last http-get or http-post.";
const char doc_wifirssi[] = "(wifi-rssi)\n"
"Returns the WiFi signal strength in dBm, or nil if not connected.";

const char doc_epdmode[] = "(epd-mode [n])\n"
"Sets the e-ink refresh mode: 1 quality, 2 text, 3 fast, 4 fastest. Returns the current mode.";
const char doc_withbatch[] = "(with-batch form*)\n"
"Evaluates the forms, holding back the e-ink refresh until the end, so many drawing calls cost one refresh.";
const char doc_refresh[] = "(refresh)\n"
"Forces an e-ink refresh of the whole screen.";
const char doc_clearscreen[] = "(clear-screen [colour])\n"
"Fills the screen with white (or colour) and homes the text cursor.";
const char doc_gray[] = "(gray level)\n"
"Returns the colour for a gray level from 0 (black) to 15 (white).";
const char doc_setfont[] = "(set-font n)\n"
"Selects a font: 0 default, 1-4 Sans 9/12/18/24, 5-6 Sans Bold 12/24, 7-8 Serif 12/24,\n"
"9-10 Mono 12/24, 11-12 DejaVu 24/56, 13 7-segment.";
const char doc_drawtext[] = "(draw-text string x y [align])\n"
"Draws string with its top edge at y, in the current font and text colour.\n"
"align: 0 left (default), 1 centred on x, 2 right. Returns the width drawn.";
const char doc_textwidth[] = "(text-width string)\n"
"Returns the width in pixels of string in the current font.";
const char doc_fontheight[] = "(font-height)\n"
"Returns the height in pixels of the current font.";
const char doc_drawellipse[] = "(draw-ellipse x y rx ry [colour])\n"
"Draws an outline ellipse centred on (x,y).";
const char doc_fillellipse[] = "(fill-ellipse x y rx ry [colour])\n"
"Draws a filled ellipse centred on (x,y).";
const char doc_fillarc[] = "(fill-arc x y r0 r1 angle0 angle1 [colour])\n"
"Draws a filled arc between radii r0 and r1, from angle0 to angle1 degrees (0 = 3 o'clock, clockwise).";
const char doc_drawqrcode[] = "(draw-qrcode string x y size)\n"
"Draws a QR code encoding string, size pixels square, with its top left corner at (x,y).";
const char doc_waittouch[] = "(wait-touch [timeout-ms])\n"
"Waits for a tap on the screen and returns (x y), or nil after timeout-ms.";
const char doc_battery[] = "(battery)\n"
"Returns the battery level in percent.";

const tbl_entry_t lookup_table2[] = {
  { "epd-mode", fn_epdmode, 0201, doc_epdmode },
  { "with-batch", sp_withbatch, 0307, doc_withbatch },
  { "refresh", fn_refresh, 0200, doc_refresh },
  { "clear-screen", fn_clearscreen, 0201, doc_clearscreen },
  { "gray", fn_gray, 0211, doc_gray },
  { "set-font", fn_setfont, 0211, doc_setfont },
  { "draw-text", fn_drawtext, 0234, doc_drawtext },
  { "text-width", fn_textwidth, 0211, doc_textwidth },
  { "font-height", fn_fontheight, 0200, doc_fontheight },
  { "draw-ellipse", fn_drawellipse, 0245, doc_drawellipse },
  { "fill-ellipse", fn_fillellipse, 0245, doc_fillellipse },
  { "fill-arc", fn_fillarc, 0267, doc_fillarc },
  { "draw-qrcode", fn_drawqrcode, 0244, doc_drawqrcode },
  { "wait-touch", fn_waittouch, 0201, doc_waittouch },
  { "battery", fn_battery, 0200, doc_battery },
  { "list-files", fn_listfiles, 0201, doc_listfiles },
  { "read-file", fn_readfile, 0211, doc_readfile },
  { "write-file", fn_writefile, 0223, doc_writefile },
  { "delete-file", fn_deletefile, 0211, doc_deletefile },
  { "make-directory", fn_makedirectory, 0211, doc_makedirectory },
  { "file-exists", fn_fileexists, 0211, doc_fileexists },
  { "sd-card-p", fn_sdcardp, 0200, doc_sdcardp },
  { "load", fn_load, 0211, doc_load },
  { "accel", fn_accel, 0200, doc_accel },
  { "gyro", fn_gyro, 0200, doc_gyro },
  { "tone", fn_tone, 0212, doc_tone },
  { "beep", fn_beep, 0202, doc_beep },
  { "volume", fn_volume, 0201, doc_volume },
  { "rtc-time", fn_rtctime, 0200, doc_rtctime },
  { "set-rtc", fn_setrtc, 0206, doc_setrtc },
  { "http-get", fn_httpget, 0211, doc_httpget },
  { "http-post", fn_httppost, 0223, doc_httppost },
  { "http-status", fn_httpstatus, 0200, doc_httpstatus },
  { "wifi-rssi", fn_wifirssi, 0200, doc_wifirssi },
  { "json-parse", fn_jsonparse, 0211, doc_jsonparse },
  { "json-string", fn_jsonstring, 0212, doc_jsonstring },
  { "json-get", fn_jsonget, 0217, doc_jsonget },
  { "http-get-json", fn_httpgetjson, 0211, doc_httpgetjson },
  { "book-open", fn_bookopen, 0211, doc_bookopen },
  { "book-next", fn_booknext, 0200, doc_booknext },
  { "book-prev", fn_bookprev, 0200, doc_bookprev },
  { "book-goto", fn_bookgoto, 0212, doc_bookgoto },
  { "book-toc", fn_booktoc, 0200, doc_booktoc },
  { "book-info", fn_bookinfo, 0200, doc_bookinfo },
  { "book-font-size", fn_bookfontsize, 0201, doc_bookfontsize },
  { "book-font", fn_bookfont, 0201, doc_bookfont },
  { "book-fonts", fn_bookfonts, 0200, doc_bookfonts },
  { "book-close", fn_bookclose, 0200, doc_bookclose },
  { "book-render", fn_bookrender, 0200, doc_bookrender },
  { "draw-ttf", fn_drawttf, 0247, doc_drawttf },
  { "ttf-width", fn_ttfwidth, 0223, doc_ttfwidth },
  { "wake-reason", fn_wakereason, 0200, doc_wakereason },
  { "light-sleep", fn_lightsleep, 0202, doc_lightsleep },
  { "deep-sleep", fn_deepsleep, 0201, doc_deepsleep },
  { "power-off", fn_poweroff, 0201, doc_poweroff },
  { "wifi-wait", fn_wifiwait, 0201, doc_wifiwait },
};

tbl_entry_t *tables[] = {lookup_table, lookup_table2};
const unsigned int tablesizes[] = { arraysize(lookup_table), arraysize(lookup_table2) };

const tbl_entry_t *table (int n) {
  return tables[n];
}

unsigned int tablesize (int n) {
  return tablesizes[n];
}
