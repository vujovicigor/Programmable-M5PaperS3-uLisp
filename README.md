# Programmable M5PaperS3 with uLisp

[uLisp](http://www.ulisp.com/) 4.9a running on the [M5Stack PaperS3](https://docs.m5stack.com/en/core/PaperS3) e-ink tablet.
It has a 250,000-object workspace in PSRAM, a Lisp library for the display, touch, IMU, buzzer, clock, files and HTTP,
and a browser IDE served over WiFi with a REPL, an editor, a file manager, docs and examples.

## Getting started

1. Install PlatformIO Core. The pioarduino ESP32 platform needs PlatformIO 6.2.0 or newer. The PyPI 6.2.0 build
   reports itself as `6.2.0.post1`, which the platform rejects, so install the develop branch in a Python 3.10–3.14 venv:
   ```
   python3 -m venv ~/.platformio/penv
   ~/.platformio/penv/bin/pip install https://github.com/platformio/platformio-core/archive/develop.zip
   ```
2. Connect the PaperS3 over USB, set `upload_port`/`monitor_port` in `platformio.ini`, then run `pio run -t upload`.
   Close any serial monitor before flashing.
3. Open the serial REPL with `pio device monitor` and connect to WiFi once (2.4 GHz only):
   ```lisp
   (wifi-connect "ssid" "password")
   ```
   The board saves the network and reconnects on every boot. The address appears on serial and along the bottom of the screen.
4. Browse to `http://<board-ip>/` or `http://ulisp.local/`.
5. After the first USB flash you can update over WiFi: `pio run -e ota -t upload`. The board must be idle
   at the uLisp prompt (no program running); it shows "Updating firmware over WiFi..." and restarts.
   If `ulisp.local` doesn't resolve, set `upload_port` in `[env:ota]` to the board's IP address.
   WiFi updates have no password, but anyone on your network can already run code through the web REPL.

## Project layout
- `patched/ulisp-esp.ino`: upstream uLisp ESP 4.9a ([technoblogy/ulisp-esp](https://github.com/technoblogy/ulisp-esp)
  @ `aa9b24c`), plus a PaperS3 board block (`M5PAPERS3`), e-ink default colours, the line editor, extensions and web hooks
- `patched/paper-ext.inc`: the PaperS3 Lisp library (the second uLisp lookup table)
- `patched/webrepl.inc`: the web server, `/eval` and the file API
- `src/webpage.h`: the browser IDE (a single HTML page)
- `patched/reader.inc`: the EPUB engine; `patched/library.lisp`: Lisp built into the firmware (the reader UI),
  embedded by `tools/mkproto.py`
- `src/ulisp.cpp`: **generated**; don't edit it. After changing anything in `patched/`, regenerate it with
  `python3 tools/mkproto.py patched/ulisp-esp.ino src/ulisp.cpp`. PlatformIO's .ino converter can't handle uLisp,
  so this script adds the forward declarations instead.
- `tools/send.py`: sends expressions over serial, e.g. `~/.platformio/penv/bin/python tools/send.py "" "(+ 1 2)"`

The platform is pinned to pioarduino 55.03.312. The USB upload speed is 115200. If USB uploads fail with "Serial data stream stopped", connect the board
directly to the computer with a different cable; a flaky USB link caused this before. Then power-cycle the board.

## WiFi / web REPL
Run `(wifi-connect "ssid" "password")` once over serial; the ESP32 saves the network and reconnects on boot.
The address is printed on serial and along the bottom of the screen.
Web requests are answered only while the serial REPL sits at its prompt (uLisp is single-threaded).
Stop a runaway web program by sending `~` over serial, or press reset.

## Web API
`POST /eval` (text/plain Lisp source, returns the output) · `GET /api/info` · `GET /api/docs` ·
`GET /api/ls?path=` · `GET|POST|DELETE /api/file?path=` (text) · `POST /api/upload?dir=` (binary, multipart) ·
`POST /api/mkdir?path=` · `GET /api/screen` (BMP screenshot).
Paths under `/sd/` are on the SD card, everything else is in internal flash (LittleFS, about 3.4 MB).
Examples live in `src/webpage.h` as `<script type="text/plain" class="example">` blocks.

## Other PaperS3 functions
| Function | |
|---|---|
| `(accel)`, `(gyro)` | IMU readings `(x y z)` in g and in degrees per second |
| `(beep [freq ms])`, `(tone freq [ms])`, `(volume [0-255])` | buzzer |
| `(rtc-time)`, `(set-rtc [y m d h mi s])` | battery-backed clock; `(set-rtc)` copies the NTP time |
| `(list-files [dir])`, `(read-file p)`, `(write-file p str [append])`, `(delete-file p)`, `(make-directory p)`, `(file-exists p)`, `(sd-card-p)` | files |
| `(load path)` | evaluate a Lisp file |
| `(http-get url)`, `(http-post url body [type])`, `(http-status)`, `(wifi-rssi)` | network (https OK, certificates not verified) |
| `(json-parse str)`, `(json-string data [pretty])`, `(json-get data key-or-index ...)`, `(http-get-json url)` | JSON: objects become `(("key" . value) ...)`, arrays become lists; `true` → `t`, `false`/`null` → `nil` |

## EPUB reader
Put `.epub` files in `/sd/books` and fonts in `/sd/fonts` (or `/fonts` on flash). Fonts must have TrueType
outlines, which most `.ttf` and many `.otf` files do, and be named `Family-Regular`, `Family-Bold`,
`Family-Italic` and `Family-BoldItalic`. Then run `(reader)`. While reading, tap the right side for the
next page, the left third for the previous page, and the top strip for the menu. The menu has contents,
text size, font, library and exit. Your position is saved next to each book as `<book>.epub.ulisp`;
font and size are shared by all books and saved in `/reader.cfg` on flash. A family's Medium weight is used
when it has one, because heavier strokes read better on e-ink; the default is Crimson Pro at 28 px. Code uses
the first font with `Mono` in its name; this setup uses DejaVu Sans Mono in `/sd/fonts`.

Upload books and fonts from the web IDE: **Editor & Files** → open the folder → **Upload**
(`POST /api/upload?dir=/sd/books`, multipart, binary-safe).

| Function | |
|---|---|
| `(reader [path])` | the touch reader (Lisp, in `patched/library.lisp`) |
| `(book-open path)`, `(book-close)` | open (portrait, last position) / close (back to landscape) |
| `(book-next)`, `(book-prev)`, `(book-goto chapter [page])`, `(book-render)` | navigation; return `(chapter page pages)` |
| `(book-toc)`, `(book-info)` | contents as `((title . chapter) ...)`, details as an association list |
| `(book-font [family])`, `(book-fonts)`, `(book-font-size [px])` | typography; changes keep your place |
| `(draw-ttf str x y size [align style gray max-width])`, `(ttf-width str size [style])` | text in the reader font, for your own screens |

The engine is in `patched/reader.inc`. It reads the zip with the ESP32-S3 ROM inflater, parses the OPF,
NCX or nav and XHTML (paragraphs, headings, lists, quotes, code, bold, italic, images), renders glyphs
with FreeType from the OpenFontRender library on 16 grays, and justifies lines. It does not hyphenate,
and supports CSS only through tag defaults.

`GET /api/screen` returns the current screen as a BMP; the web IDE has a **Screenshot** link.

## Display library (PaperS3 additions)
Colours are RGB565 numbers; the panel shows 16 grays. `0` is black, `#xffff` is white.
On this board uLisp's default drawing colour is black and `(fill-screen)` clears to white.

| Function | |
|---|---|
| `(with-batch form*)` | run drawing forms, refresh the e-ink once at the end |
| `(epd-mode [n])` | 1 quality (default), 2 text, 3 fast, 4 fastest |
| `(refresh)` | force a full refresh |
| `(clear-screen [colour])` | fill white (or colour), home cursor |
| `(gray level)` | colour for gray 0 (black) to 15 (white) |
| `(set-font n)` | 0 default, 1-4 Sans 9/12/18/24, 5-6 Sans Bold 12/24, 7-8 Serif 12/24, 9-10 Mono 12/24, 11-12 DejaVu 24/56, 13 7-segment |
| `(draw-text str x y [align])` | align 0 left, 1 centre, 2 right; y = top edge |
| `(text-width str)`, `(font-height)` | text metrics for the current font |
| `(draw-ellipse x y rx ry [c])`, `(fill-ellipse ...)` | ellipses |
| `(fill-arc x y r0 r1 a0 a1 [c])` | arc/ring segment, degrees, 0 = 3 o'clock, clockwise |
| `(draw-qrcode str x y size)` | QR code |
| `(wait-touch [timeout-ms])` | wait for a tap, returns `(x y)` or nil |
| `(battery)` | battery percent |

The standard uLisp graphics functions also work: `draw-pixel`, `draw-line`, `draw-rect`, `fill-rect`,
`draw-circle`, `fill-circle`, `draw-round-rect`, `fill-round-rect`, `draw-triangle`, `fill-triangle`,
`draw-char`, `set-cursor`, `set-text-color`, `set-text-size`, `set-text-wrap`, `fill-screen`,
`set-rotation`, `read-pixel`, `touchscreen`, `display-size`, `(with-gfx (s) (princ "x" s))`.
Use `(documentation 'name)` for help on any of them.

