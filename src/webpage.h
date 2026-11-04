#pragma once
const char WEBPAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>uLisp · PaperS3</title>
<style>
  :root { --bg:#f6f5f1; --panel:#fff; --fg:#1b1b1b; --muted:#77736a; --line:#dcd8cf; --accent:#2f5d8a;
          --accent-soft:#e6eef6; --err:#a33; --ok:#2e6b3a; }
  * { box-sizing:border-box; }
  html, body { height:100%; }
  body { margin:0; background:var(--bg); color:var(--fg); font:14px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
         display:flex; flex-direction:column; }
  code, pre, textarea, .mono { font-family:ui-monospace,SFMono-Regular,Menlo,monospace; font-size:13.5px; }
  header { display:flex; align-items:center; gap:4px; padding:0 12px; border-bottom:1px solid var(--line); background:var(--panel); }
  header .brand { font-weight:600; margin-right:14px; padding:10px 0; }
  header .brand span { color:var(--muted); font-weight:400; }
  .tab { border:0; background:none; font:inherit; padding:12px 12px 10px; color:var(--muted); cursor:pointer;
         border-bottom:2px solid transparent; }
  .tab.on { color:var(--fg); border-bottom-color:var(--accent); }
  header .status { margin-left:auto; color:var(--muted); font-size:12.5px; white-space:nowrap; overflow:hidden; }
  main { flex:1; min-height:0; display:flex; }
  .view { display:none; flex:1; min-height:0; min-width:0; }
  .view.on { display:flex; }
  button.b { font:inherit; font-size:13px; padding:5px 12px; border:1px solid var(--line); background:var(--panel);
             color:var(--fg); border-radius:5px; cursor:pointer; }
  button.b:hover { border-color:var(--accent); }
  button.b.primary { background:var(--accent); border-color:var(--accent); color:#fff; }
  button.b:disabled { opacity:.5; cursor:default; }
  button.b.danger:hover { border-color:var(--err); color:var(--err); }
  textarea { width:100%; border:1px solid var(--line); border-radius:5px; background:var(--panel); color:var(--fg);
             padding:10px; resize:none; tab-size:2; outline:none; }
  textarea:focus { border-color:var(--accent); }
  .out { white-space:pre-wrap; word-break:break-word; }
  .err { color:var(--err); }
  .pending { color:var(--muted); font-style:italic; }

  /* REPL */
  #repl { flex-direction:column; }
  #log { flex:1; overflow:auto; padding:12px 16px; }
  #log .in { color:var(--accent); white-space:pre-wrap; } #log .in::before { content:"> "; color:var(--muted); }
  #log .out { margin:2px 0 12px; }
  #replform { padding:6px 16px 10px; display:flex; gap:10px; background:var(--panel); }
  #replcode { height:32vh; min-height:60px; }
  #splitter { height:9px; flex:none; cursor:row-resize; background:var(--panel); border-top:1px solid var(--line);
              display:flex; align-items:center; justify-content:center; touch-action:none; }
  #splitter::before { content:""; width:44px; height:3px; border-radius:2px; background:var(--line); }
  #splitter:hover::before, #splitter.drag::before { background:var(--accent); }
  .hint { color:var(--muted); font-size:12px; }

  /* Editor */
  #editor { flex-direction:row; }
  #files { width:250px; border-right:1px solid var(--line); background:var(--panel); display:flex; flex-direction:column; }
  #files .roots { display:flex; gap:6px; padding:10px; border-bottom:1px solid var(--line); }
  #files .roots button { flex:1; }
  #files .roots button.on { background:var(--accent-soft); border-color:var(--accent); }
  #files .path { padding:8px 10px; color:var(--muted); font-size:12.5px; display:flex; justify-content:space-between; align-items:center; }
  #filelist { flex:1; overflow:auto; }
  #filelist div { padding:6px 12px; cursor:pointer; display:flex; justify-content:space-between; gap:8px; }
  #filelist div:hover { background:var(--bg); }
  #filelist div.cur { background:var(--accent-soft); }
  #filelist .size { color:var(--muted); font-size:12px; }
  #filelist .dir { font-weight:600; }
  #files .usage { padding:8px 10px; color:var(--muted); font-size:12px; border-top:1px solid var(--line); }
  #edmain { flex:1; display:flex; flex-direction:column; min-width:0; padding:10px 14px; gap:8px; }
  #edbar { display:flex; gap:6px; align-items:center; flex-wrap:wrap; }
  #fname { flex:1; min-width:160px; font:inherit; padding:5px 8px; border:1px solid var(--line); border-radius:5px; }
  #edcode { flex:3; min-height:120px; }
  #edstatus { display:flex; justify-content:space-between; color:var(--muted); font-size:12px; }
  #edout { flex:1; min-height:60px; overflow:auto; border:1px solid var(--line); border-radius:5px; background:var(--panel); padding:8px 10px; }
  #fname.dirty { border-color:var(--accent); box-shadow:inset 3px 0 0 var(--accent); }

  /* Docs */
  #docs { flex-direction:row; }
  #docnav { width:260px; border-right:1px solid var(--line); background:var(--panel); display:flex; flex-direction:column; }
  #docsearch { margin:10px; width:auto; padding:6px 8px; font:inherit; border:1px solid var(--line); border-radius:5px; }
  #doclist { flex:1; overflow:auto; }
  #doclist .grp { padding:10px 12px 4px; color:var(--muted); font-size:11.5px; text-transform:uppercase; letter-spacing:.04em; }
  #doclist a { display:block; padding:3px 12px; color:var(--fg); text-decoration:none; font-family:ui-monospace,Menlo,monospace; font-size:13px; }
  #doclist a:hover { background:var(--bg); }
  #docbody { flex:1; overflow:auto; padding:18px 28px; }
  #docbody h2 { margin:0 0 6px; font-size:20px; } #docbody h3 { margin:26px 0 6px; font-size:15px; }
  #docbody p { max-width:760px; }
  #docbody pre { background:var(--panel); border:1px solid var(--line); border-radius:5px; padding:10px 12px; overflow:auto; max-width:900px; }
  .entry { border-bottom:1px solid var(--line); padding:10px 0; max-width:900px; }
  .entry .sig { font-family:ui-monospace,Menlo,monospace; font-weight:600; }
  .entry .txt { white-space:pre-wrap; margin-top:3px; }
  .tag { font-size:11px; color:var(--accent); background:var(--accent-soft); border-radius:3px; padding:1px 5px; margin-left:6px; font-weight:400; }
  table.ref { border-collapse:collapse; } table.ref td { border-bottom:1px solid var(--line); padding:4px 14px 4px 0; vertical-align:top; }

  /* Examples */
  #examples { overflow:auto; }
  #exgrid { display:grid; grid-template-columns:repeat(auto-fill, minmax(380px, 1fr)); gap:14px; padding:16px; width:100%; align-content:start; }
  .card { background:var(--panel); border:1px solid var(--line); border-radius:7px; display:flex; flex-direction:column; }
  .card h4 { margin:0; padding:12px 14px 2px; font-size:15px; }
  .card p { margin:0; padding:2px 14px 8px; color:var(--muted); }
  .card pre { margin:0 14px; padding:10px; background:var(--bg); border-radius:5px; max-height:220px; overflow:auto; font-size:12.5px; }
  .card .acts { display:flex; gap:6px; padding:10px 14px 12px; }
  .card .res { margin:0 14px 12px; font-size:12.5px; max-height:140px; overflow:auto; }
</style></head><body>
<header>
  <div class="brand">uLisp <span>· PaperS3</span></div>
  <button class="tab on" data-v="repl">REPL</button>
  <button class="tab" data-v="editor">Editor &amp; Files</button>
  <button class="tab" data-v="docs">Docs</button>
  <button class="tab" data-v="examples">Examples</button>
  <a class="tab" href="/api/screen" target="_blank" title="What the e-ink screen shows right now" style="text-decoration:none">Screenshot</a>
  <div class="status" id="status"></div>
</header>
<main>
  <section class="view on" id="repl">
    <div id="log"></div>
    <div id="splitter" title="Drag to resize · double-click to reset"></div>
    <form id="replform">
      <textarea id="replcode" spellcheck="false" autofocus placeholder="(+ 1 2 3)"></textarea>
      <div style="display:flex;flex-direction:column;gap:6px;justify-content:space-between">
        <button class="b primary" id="replrun">Run</button>
        <button class="b" type="button" id="replclear">Clear</button>
      </div>
    </form>
    <div class="hint" style="padding:0 16px 8px;background:var(--panel)">⌘/Ctrl+Enter runs · ↑/↓ history (single-line input) · several forms at once are fine · runaway loop: press reset, or send <code>~</code> over serial</div>
  </section>

  <section class="view" id="editor">
    <div id="files">
      <div class="roots"><button class="b on" data-root="/">Flash</button><button class="b" data-root="/sd" id="sdroot">SD card</button></div>
      <div class="path"><span id="curdir" class="mono">/</span><span><button class="b" id="newdir" title="New folder" style="padding:2px 8px">+ folder</button></span></div>
      <div id="filelist"></div>
      <div class="usage" id="usage"></div>
    </div>
    <div id="edmain">
      <div id="edbar">
        <input id="fname" class="mono" spellcheck="false" placeholder="/program.lisp">
        <button class="b" id="ednew">New</button>
        <button class="b" id="edsave" title="⌘/Ctrl+S">Save</button>
        <button class="b primary" id="edrun" title="⌘/Ctrl+Enter: runs the selection, or the whole file">Run</button>
        <button class="b" id="edload" title="Save, then evaluate the file on the device with (load ...)">Save &amp; load</button>
        <button class="b danger" id="eddel">Delete</button>
      </div>
      <textarea id="edcode" spellcheck="false" placeholder="; Write a program here, then Save (⌘S) and Run (⌘Enter)."></textarea>
      <div id="edstatus"><span id="edmsg"></span><span id="parens"></span></div>
      <div id="edout" class="out mono"></div>
    </div>
  </section>

  <section class="view" id="docs">
    <div id="docnav">
      <input id="docsearch" placeholder="Search functions…" spellcheck="false">
      <div id="doclist"></div>
    </div>
    <div id="docbody"></div>
  </section>

  <section class="view" id="examples"><div id="exgrid"></div></section>
</main>

<template id="guide">
  <h2>uLisp on the M5Stack PaperS3</h2>
  <p>uLisp is a small Lisp for microcontrollers. This firmware adds functions for the PaperS3: the e-ink display, touch, the motion sensor, the buzzer, the clock, files on flash and SD card, and HTTP. Search on the left for any function, or see the full language reference at <a href="http://www.ulisp.com/show?3L" target="_blank">ulisp.com</a>.</p>
  <h3>The screen</h3>
  <p>960 × 540 pixels, landscape, origin at the top left. Colours are 16-bit numbers. The panel shows 16 grays, so use <code>0</code> for black, <code>#xffff</code> for white, or <code>(gray 0…15)</code>. Drawing functions default to black.</p>
  <p>Every drawing call refreshes the e-ink, which takes about half a second in quality mode. To draw many things with one refresh, wrap them in <code>with-batch</code>:</p>
<pre>(with-batch
  (clear-screen)
  (set-font 4)
  (draw-text "Hello" 480 200 1)        ; 1 = centred on x
  (fill-circle 480 380 60 (gray 6)))</pre>
  <p>For animation and touch, switch to a faster refresh with <code>(epd-mode 4)</code>, and go back to quality with <code>(epd-mode 1)</code>.</p>
  <h3>PaperS3 functions</h3>
  <table class="ref">
    <tr><td>Display</td><td><code>with-batch epd-mode refresh clear-screen gray set-font draw-text text-width font-height draw-ellipse fill-ellipse fill-arc draw-qrcode</code></td></tr>
    <tr><td>uLisp graphics</td><td><code>draw-pixel draw-line draw-rect fill-rect draw-circle fill-circle draw-round-rect fill-round-rect draw-triangle fill-triangle draw-char set-cursor set-text-color set-text-size set-text-wrap fill-screen read-pixel display-size with-gfx</code></td></tr>
    <tr><td>Touch</td><td><code>touchscreen wait-touch</code></td></tr>
    <tr><td>Sensors</td><td><code>accel gyro battery</code></td></tr>
    <tr><td>Sound</td><td><code>beep tone volume</code></td></tr>
    <tr><td>Clock</td><td><code>rtc-time set-rtc get-time</code></td></tr>
    <tr><td>Files</td><td><code>list-files read-file write-file delete-file make-directory file-exists sd-card-p load save-image load-image</code></td></tr>
    <tr><td>Network</td><td><code>http-get http-post http-status wifi-rssi wifi-localip wifi-connect</code></td></tr>
    <tr><td>JSON</td><td><code>json-parse json-string json-get http-get-json</code></td></tr>
    <tr><td>Books</td><td><code>reader book-open book-next book-prev book-goto book-toc book-info book-font book-fonts book-font-size book-render book-close draw-ttf ttf-width</code></td></tr>
  </table>
  <h3>Files</h3>
  <p>Paths starting with <code>/sd/</code> are on the SD card, and anything else is on the internal flash (about 3 MB). The <b>Editor &amp; Files</b> tab saves and opens programs. <b>Save &amp; load</b> runs a file on the device with <code>(load "/file.lisp")</code>, so its definitions stay available in the REPL.</p>
  <h3>Reading EPUB books</h3>
  <p>Put <code>.epub</code> files in <code>/sd/books</code> and fonts (<code>.ttf</code> or <code>.otf</code> with TrueType outlines, named like <code>Family-Regular.ttf</code>, <code>Family-Bold.ttf</code>, <code>Family-Italic.ttf</code>) in <code>/sd/fonts</code>. Run <code>(reader)</code> for the library, or <code>(reader "/sd/books/name.epub")</code>. While reading, tap the right side for the next page, the left third for the previous page, and the top strip for the menu (contents, text size, font, library, exit). The reader remembers your page in each book. It is written in Lisp on top of the <code>book-</code> functions, so you can change it: see <code>patched/library.lisp</code> in the project.</p>
  <h3>Things to know</h3>
  <p>The browser and the USB serial REPL share one interpreter. A web request is answered only while the serial REPL is idle, and a long-running program blocks everything until it finishes. To stop a runaway program, press the reset button, or send <code>~</code> over the serial monitor. Definitions live in RAM until reset; keep programs in files and <code>load</code> them.</p>
</template>

<!-- Examples: each block is a runnable program. data-title / data-desc describe it. -->
<script type="text/plain" class="example" data-title="Hello, PaperS3" data-desc="Fonts, alignment and with-batch (one e-ink refresh).">
(with-batch
  (clear-screen)
  (set-text-color 0 #xffff)
  (set-font 6)
  (draw-text "Hello, PaperS3!" 480 170 1)
  (set-font 3)
  (draw-text "uLisp on e-ink" 480 250 1)
  (set-font 9)
  (draw-text "(+ 1 2 3) => 6" 480 330 1)
  (set-font 0))
</script>
<script type="text/plain" class="example" data-title="Bar chart" data-desc="Draws a labelled bar chart from a list of numbers, in shades of gray.">
(defun bar-chart (values)
  (let* ((n (length values))
         (w (truncate 860 n))
         (top (apply max values))
         (i 0))
    (with-batch
      (clear-screen)
      (set-font 3)
      (draw-text "Bar chart" 480 20 1)
      (set-font 2)
      (dolist (v values)
        (let ((h (truncate (* 380 v) top))
              (x (+ 50 (* i w))))
          (fill-rect x (- 510 h) (- w 12) h (gray (* 2 (mod i 7))))
          (draw-text (princ-to-string v) (+ x (truncate (- w 12) 2)) (- 500 h 32) 1))
        (incf i))
      (set-font 0))))

(bar-chart '(3 7 4 9 12 6 8 5))
</script>
<script type="text/plain" class="example" data-title="Sierpinski triangle" data-desc="Recursion: 729 filled triangles drawn in a single refresh.">
(defun mid (a b) (truncate (+ a b) 2))

(defun sierpinski (x1 y1 x2 y2 x3 y3 depth)
  (if (= depth 0)
      (fill-triangle x1 y1 x2 y2 x3 y3 0)
      (let ((ax (mid x1 x2)) (ay (mid y1 y2))
            (bx (mid x2 x3)) (by (mid y2 y3))
            (cx (mid x1 x3)) (cy (mid y1 y3)))
        (sierpinski x1 y1 ax ay cx cy (1- depth))
        (sierpinski ax ay x2 y2 bx by (1- depth))
        (sierpinski cx cy bx by x3 y3 (1- depth)))))

(with-batch
  (clear-screen)
  (sierpinski 480 20 60 520 900 520 6))
</script>
<script type="text/plain" class="example" data-title="String art" data-desc="Floating-point maths with sin and cos: lines between points on a circle.">
(defun pt (a r)
  (list (+ 480 (round (* r (cos a))))
        (+ 270 (round (* r (sin a))))))

(with-batch
  (clear-screen)
  (dotimes (i 120)
    (let* ((a (* i 0.05236))           ; 2*pi/120
           (p (pt a 250))
           (q (pt (* 3 a) 250)))
      (draw-line (first p) (second p) (first q) (second q) 0))))
</script>
<script type="text/plain" class="example" data-title="Spirit level" data-desc="Tilt the PaperS3 and the bubble follows the accelerometer. Runs for about 20 seconds in fast refresh mode.">
(defun bubble (steps)
  (epd-mode 4)
  (with-batch
    (clear-screen)
    (draw-circle 480 270 220 0)
    (draw-circle 480 270 30 0))
  (let ((ox 480) (oy 270))
    (dotimes (i steps)
      (let* ((a (accel))
             (x (+ 480 (round (* 200 (first a)))))
             (y (- 270 (round (* 200 (second a))))))
        (with-batch
          (fill-circle ox oy 22 #xffff)
          (draw-circle 480 270 30 0)
          (fill-circle x y 20 0))
        (setq ox x oy y)
        (delay 150))))
  (epd-mode 1)
  (accel))

(bubble 80)
</script>
<script type="text/plain" class="example" data-title="Sketchpad" data-desc="Draw with your finger. Tap the 'done' box in the top-left corner to stop.">
(defun sketch ()
  (epd-mode 4)
  (with-batch
    (clear-screen)
    (draw-rect 0 0 110 56 0)
    (set-font 3)
    (draw-text "done" 55 14 1)
    (set-font 0))
  (loop
    (let ((p (touchscreen)))
      (when p
        (if (and (< (first p) 110) (< (second p) 56))
            (return)
            (fill-circle (first p) (second p) 6 0)))))
  (epd-mode 1)
  'done)

(sketch)
</script>
<script type="text/plain" class="example" data-title="Tap targets" data-desc="wait-touch: tap the circles; each tap beeps and reports where you hit.">
(defun tap-game (rounds)
  (let ((hits 0))
    (dotimes (i rounds)
      (let ((x (+ 100 (random 760)))
            (y (+ 100 (random 340))))
        (with-batch
          (clear-screen)
          (set-font 3)
          (draw-text (format nil "Round ~a of ~a: tap the circle" (1+ i) rounds) 480 20 1)
          (fill-circle x y 50 0))
        (let ((p (wait-touch 10000)))
          (when (and p (< (abs (- (first p) x)) 60) (< (abs (- (second p) y)) 60))
            (incf hits)
            (beep 2500 80)))))
    (with-batch
      (clear-screen)
      (set-font 6)
      (draw-text (format nil "~a of ~a hits" hits rounds) 480 230 1)
      (set-font 0))
    hits))

(tap-game 5)
</script>
<script type="text/plain" class="example" data-title="Melody" data-desc="Plays notes on the buzzer. Each note is (frequency milliseconds); 0 is a rest.">
(defun play (notes)
  (dolist (n notes)
    (if (= (first n) 0)
        (delay (second n))
        (beep (first n) (second n)))
    (delay 25)))

(play '((262 180) (294 180) (330 180) (349 180)
        (392 360) (0 60) (392 360)
        (440 180) (440 180) (440 180) (440 180) (392 600)))
</script>
<script type="text/plain" class="example" data-title="Clock" data-desc="Copies network time into the battery-backed clock, then shows the time (UTC).">
(defun two (n) (if (< n 10) (format nil "0~a" n) (format nil "~a" n)))

(defun show-clock ()
  (let ((tm (rtc-time)))
    (with-batch
      (clear-screen)
      (set-font 13)
      (set-text-size 3)
      (draw-text (format nil "~a:~a" (two (nth 3 tm)) (two (nth 4 tm))) 480 110 1)
      (set-text-size 1)
      (set-font 3)
      (draw-text (format nil "~a-~a-~a  UTC" (nth 0 tm) (two (nth 1 tm)) (two (nth 2 tm))) 480 400 1)
      (set-font 0))
    tm))

(set-rtc)
(show-clock)
</script>
<script type="text/plain" class="example" data-title="Fetch from the internet" data-desc="http-get over HTTPS, then shows the text on the screen.">
(let ((s (http-get "https://api.github.com/zen")))
  (with-batch
    (clear-screen)
    (set-font 2)
    (draw-text "GitHub says:" 480 170 1)
    (set-font 4)
    (draw-text s 480 230 1)
    (set-font 0))
  (list (http-status) s))
</script>
<script type="text/plain" class="example" data-title="JSON basics" data-desc="Parse JSON into lists, pick values out with json-get, and turn Lisp data back into JSON.">
(defvar data
  (json-parse "{\"name\": \"PaperS3\", \"size\": [960, 540], \"eink\": true, \"price\": null}"))

data                          ; objects become (("key" . value) ...)
(json-get data "size" 0)      ; path of keys and list indexes
(json-string data)
(json-string '(("list" 1 2 3) ("nested" ("a" . 1) ("b" . 2.5))) t)
</script>
<script type="text/plain" class="example" data-title="Weather" data-desc="Current weather from open-meteo.com (no API key) with http-get-json, shown on the screen. Edit the coordinates for your location.">
; Change these to your location (these are Berlin).
(defvar *lat* "52.52")
(defvar *lon* "13.41")

(defun fetch-weather ()
  (http-get-json
    (format nil "https://api.open-meteo.com/v1/forecast?latitude=~a&longitude=~a&current=temperature_2m,relative_humidity_2m,wind_speed_10m&timezone=auto"
            *lat* *lon*)))

(let* ((w (fetch-weather))
       (temp (json-get w "current" "temperature_2m"))
       (hum (json-get w "current" "relative_humidity_2m"))
       (wind (json-get w "current" "wind_speed_10m"))
       (updated (json-get w "current" "time")))
  (with-batch
    (clear-screen)
    (set-font 3)
    (draw-text "Weather" 480 40 1)
    (set-font 12)
    (draw-text (format nil "~a C" temp) 480 140 1)
    (set-font 3)
    (draw-text (format nil "humidity ~a%    wind ~a km/h" hum wind) 480 300 1)
    (set-font 2)
    (draw-text (format nil "updated ~a" updated) 480 420 1)
    (set-font 0))
  (list temp hum wind))
</script>
<script type="text/plain" class="example" data-title="Files" data-desc="Writes a program to flash, lists the files, then loads and calls it.">
(write-file "/hello.lisp"
  "(defun greet (name) (format nil \"Hello, ~a!\" name))")
(print (list-files "/"))
(load "/hello.lisp")
(greet "PaperS3")
</script>
<script type="text/plain" class="example" data-title="Device dashboard" data-desc="Battery, WiFi, memory and storage, shown on the screen.">
(with-batch
  (clear-screen)
  (set-font 6)
  (draw-text "PaperS3" 40 30)
  (set-font 3)
  (draw-text (format nil "Battery   ~a%" (battery)) 40 120)
  (draw-text (format nil "WiFi      ~a dBm" (wifi-rssi)) 40 180)
  (draw-text (format nil "Address   ~a" (wifi-localip)) 40 240)
  (draw-text (format nil "Memory    ~a objects free" (room)) 40 300)
  (draw-text (format nil "SD card   ~a" (if (sd-card-p) "inserted" "none")) 40 360)
  (draw-text (format nil "Tilt      ~a" (mapcar round (accel))) 40 420)
  (set-font 0))
</script>
<script type="text/plain" class="example" data-title="EPUB reader" data-desc="Opens the library of books in /sd/books. Tap right for next page, the left third for previous, the top for the menu. This page waits until you choose Exit reader.">
(reader)
</script>
<script type="text/plain" class="example" data-title="Books from Lisp" data-desc="The reader's building blocks: open a book, look at its contents, and turn pages from code.">
(book-open "/sd/books/Austen, Jane - Pride and Prejudice.epub")
(book-info)
(subseq (book-toc) 0 5)
(book-goto 2)          ; chapter index from book-toc
(book-next)            ; returns (chapter page pages)
(book-fonts)
</script>
<script type="text/plain" class="example" data-title="Lisp basics" data-desc="Recursion, lambdas, mapcar and timing.">
(defun fib (n)
  (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))

(mapcar fib '(0 1 2 3 4 5 6 7 8 9 10))
(mapcar (lambda (x) (* x x)) '(1 2 3 4 5))
(let ((words '("e-ink" "lisp" "paper")))
  (sort words string<))
(time (fib 18))
</script>

<script>
const $ = s => document.querySelector(s);
const esc = s => s.replace(/[&<>]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
let busy = false;

// ---- Tabs ----
function show(v) {
  document.querySelectorAll('.tab').forEach(t => t.classList.toggle('on', t.dataset.v === v));
  document.querySelectorAll('.view').forEach(s => s.classList.toggle('on', s.id === v));
  if (v === 'docs') loadDocs();
  if (v === 'editor') refreshFiles();
  location.hash = v;
}
document.querySelectorAll('.tab').forEach(t => t.onclick = () => show(t.dataset.v));

// ---- Device ----
async function evalLisp(src) {
  busy = true;
  try {
    const r = await fetch('/eval', { method:'POST', headers:{'Content-Type':'text/plain'}, body:src });
    return (await r.text()).replace(/\r/g, '').replace(/\n+$/, '');
  } finally { busy = false; updStatus(); }
}
const isErr = t => /^Error/m.test(t);
async function updStatus() {
  try {
    const i = await (await fetch('/api/info')).json();
    $('#status').textContent = `${i.free} objects free · battery ${i.battery}% · WiFi ${i.rssi} dBm` + (i.sd ? ' · SD card' : '');
    $('#sdroot').disabled = !i.sd;
    $('#usage').textContent = root === '/'
      ? `Flash: ${(i.flashUsed/1024).toFixed(0)} of ${(i.flashTotal/1024).toFixed(0)} KB used`
      : (i.sd ? `SD: ${(i.sdUsed/1048576).toFixed(0)} of ${(i.sdTotal/1048576).toFixed(0)} MB used` : '');
  } catch {}
}

// ---- REPL ----
const log = $('#log'), rc = $('#replcode');
let hist = JSON.parse(localStorage.getItem('ulisp-hist') || '[]'), hpos = hist.length;
function addLog(cls, text) { const d = document.createElement('div'); d.className = cls; d.textContent = text;
  log.appendChild(d); log.scrollTop = log.scrollHeight; return d; }
async function replRun(src) {
  src = (src ?? rc.value).trim(); if (!src || busy) return;
  addLog('in', src); hist.push(src); hist = hist.slice(-200); hpos = hist.length;
  localStorage.setItem('ulisp-hist', JSON.stringify(hist));
  rc.value = '';
  const out = addLog('out pending', 'evaluating…');
  try { const t = await evalLisp(src); out.className = 'out' + (isErr(t) ? ' err' : ''); out.textContent = t; }
  catch (e) { out.className = 'out err'; out.textContent = 'connection error: ' + e; }
  rc.focus(); log.scrollTop = log.scrollHeight;
}
$('#replform').onsubmit = e => { e.preventDefault(); replRun(); };
$('#replclear').onclick = () => { log.innerHTML = ''; rc.focus(); };
rc.addEventListener('keydown', e => {
  if (e.key === 'Enter' && (e.metaKey || e.ctrlKey)) { e.preventDefault(); replRun(); }
  else if ((e.key === 'ArrowUp' || e.key === 'ArrowDown') && !rc.value.includes('\n')) {
    e.preventDefault();
    hpos = Math.max(0, Math.min(hist.length, hpos + (e.key === 'ArrowUp' ? -1 : 1)));
    rc.value = hist[hpos] || '';
  } else lispKeys(e, rc);
});

// Shared editing keys: Tab indents, Enter keeps indentation
function lispKeys(e, ta) {
  if (e.key === 'Tab' && !e.shiftKey) {
    e.preventDefault(); document.execCommand('insertText', false, '  ');
  } else if (e.key === 'Enter' && !e.metaKey && !e.ctrlKey && !e.shiftKey) {
    const before = ta.value.slice(0, ta.selectionStart);
    const line = before.slice(before.lastIndexOf('\n') + 1);
    // indent to the innermost open paren + 2, or keep the current indentation
    let depth = 0, col = -1;
    for (let i = before.length - 1; i >= 0; i--) {
      const c = before[i];
      if (c === ')') depth++;
      else if (c === '(') { if (depth === 0) { col = i - before.lastIndexOf('\n', i) - 1; break; } depth--; }
    }
    const indent = col >= 0 ? col + 2 : line.match(/^\s*/)[0].length;
    e.preventDefault(); document.execCommand('insertText', false, '\n' + ' '.repeat(indent));
  }
}

// ---- Editor & files ----
let root = '/', cwd = '/', curFile = '', dirty = false;
const ed = $('#edcode'), fname = $('#fname'), edout = $('#edout');
const join = (d, n) => (d.endsWith('/') ? d : d + '/') + n;
function setMsg(t, err) { $('#edmsg').textContent = t; $('#edmsg').className = err ? 'err' : ''; }
function setDirty(d) { dirty = d; fname.classList.toggle('dirty', d); }
function parenCheck() {
  let depth = 0, str = false, line = 1, bad = 0;
  const s = ed.value;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (c === '\n') line++;
    if (str) { if (c === '\\') i++; else if (c === '"') str = false; continue; }
    if (c === ';') { while (i < s.length && s[i] !== '\n') i++; line++; continue; }
    if (c === '#' && s[i+1] === '\\') { i += 2; continue; }
    if (c === '"') str = true;
    else if (c === '(') depth++;
    else if (c === ')') { depth--; if (depth < 0 && !bad) bad = line; }
  }
  const el = $('#parens');
  if (bad) { el.textContent = `extra ) on line ${bad}`; el.className = 'err'; }
  else if (depth > 0) { el.textContent = `${depth} unclosed (`; el.className = 'err'; }
  else { el.textContent = 'parentheses balanced'; el.className = ''; }
}
ed.addEventListener('input', () => { setDirty(true); parenCheck(); });
ed.addEventListener('keydown', e => {
  if (e.key === 'Enter' && (e.metaKey || e.ctrlKey)) { e.preventDefault(); edRun(); }
  else if (e.key === 's' && (e.metaKey || e.ctrlKey)) { e.preventDefault(); edSave(); }
  else lispKeys(e, ed);
});
document.addEventListener('keydown', e => {
  if (e.key === 's' && (e.metaKey || e.ctrlKey) && $('#editor').classList.contains('on')) { e.preventDefault(); edSave(); }
});

async function refreshFiles() {
  updStatus();
  $('#curdir').textContent = cwd;
  const list = $('#filelist'); list.innerHTML = '';
  let items;
  try { const r = await fetch('/api/ls?path=' + encodeURIComponent(cwd)); if (!r.ok) throw await r.text(); items = await r.json(); }
  catch (e) { list.innerHTML = `<div class="err">${esc(String(e))}</div>`; return; }
  items.sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name));
  if (cwd !== root) items.unshift({ name:'..', dir:true, up:true });
  for (const it of items) {
    const d = document.createElement('div');
    const path = it.up ? cwd.slice(0, cwd.lastIndexOf('/')) || root : join(cwd, it.name);
    d.innerHTML = `<span class="${it.dir ? 'dir' : ''}">${esc(it.name)}${it.dir && !it.up ? '/' : ''}</span>` +
                  (it.dir ? '' : `<span class="size">${it.size < 1024 ? it.size + ' B' : (it.size/1024).toFixed(1) + ' KB'}</span>`);
    if (path === curFile) d.classList.add('cur');
    d.onclick = () => it.dir ? (cwd = path.length < root.length ? root : path, refreshFiles()) : openFile(path);
    list.appendChild(d);
  }
  if (!items.length) list.innerHTML = '<div class="hint" style="cursor:default">No files yet. Write something and press Save.</div>';
}
document.querySelectorAll('[data-root]').forEach(b => b.onclick = () => {
  root = cwd = b.dataset.root;
  document.querySelectorAll('[data-root]').forEach(x => x.classList.toggle('on', x === b));
  refreshFiles();
});
$('#newdir').onclick = async () => {
  const n = prompt('New folder name'); if (!n) return;
  const r = await fetch('/api/mkdir?path=' + encodeURIComponent(join(cwd, n)), { method:'POST' });
  setMsg(await r.text(), !r.ok); refreshFiles();
};
async function openFile(path) {
  if (dirty && !confirm('Discard unsaved changes?')) return;
  const r = await fetch('/api/file?path=' + encodeURIComponent(path));
  if (!r.ok) { setMsg(await r.text(), true); return; }
  ed.value = await r.text(); curFile = path; fname.value = path; setDirty(false); parenCheck();
  setMsg('Opened ' + path); refreshFiles();
}
function targetPath() {
  let p = fname.value.trim();
  if (!p) return null;
  if (!p.startsWith('/')) p = join(cwd, p);
  if (!/\.[a-z0-9]+$/i.test(p)) p += '.lisp';
  return p;
}
async function edSave() {
  const p = targetPath();
  if (!p) { setMsg('Enter a file name first', true); fname.focus(); return false; }
  const r = await fetch('/api/file?path=' + encodeURIComponent(p), { method:'POST', headers:{'Content-Type':'text/plain'}, body:ed.value });
  if (!r.ok) { setMsg('Save failed: ' + await r.text(), true); return false; }
  curFile = p; fname.value = p; setDirty(false); setMsg('Saved ' + p + ' (' + await r.text() + ' bytes)');
  refreshFiles(); return true;
}
async function showRun(src, label) {
  if (busy) return;
  edout.className = 'out mono pending'; edout.textContent = label;
  try { const t = await evalLisp(src); edout.className = 'out mono' + (isErr(t) ? ' err' : ''); edout.textContent = t || '(no output)'; }
  catch (e) { edout.className = 'out mono err'; edout.textContent = 'connection error: ' + e; }
}
function edRun() {
  const sel = ed.value.slice(ed.selectionStart, ed.selectionEnd);
  showRun(sel.trim() ? sel : ed.value, sel.trim() ? 'running selection…' : 'running…');
}
$('#edrun').onclick = edRun;
$('#edsave').onclick = edSave;
$('#edload').onclick = async () => { if (await edSave()) showRun(`(load "${curFile}")`, 'loading ' + curFile + '…'); };
$('#ednew').onclick = () => {
  if (dirty && !confirm('Discard unsaved changes?')) return;
  ed.value = ''; curFile = ''; fname.value = join(cwd, 'untitled.lisp'); setDirty(false); parenCheck(); edout.textContent = '';
  fname.focus(); fname.select();
};
$('#eddel').onclick = async () => {
  const p = curFile || targetPath(); if (!p) return;
  if (!confirm('Delete ' + p + '?')) return;
  const r = await fetch('/api/file?path=' + encodeURIComponent(p), { method:'DELETE' });
  setMsg(await r.text(), !r.ok);
  if (r.ok) { curFile = ''; setDirty(true); }
  refreshFiles();
};

// ---- Docs ----
let docs = null;
async function loadDocs() {
  if (docs) return;
  $('#docbody').innerHTML = $('#guide').innerHTML;
  try { docs = await (await fetch('/api/docs')).json(); } catch { docs = []; }
  renderDocList('');
}
function renderDocList(q) {
  q = q.toLowerCase();
  const list = $('#doclist'); list.innerHTML = '';
  const guide = document.createElement('a'); guide.textContent = 'Guide'; guide.href = '#';
  guide.style.fontFamily = 'inherit'; guide.onclick = e => { e.preventDefault(); $('#docbody').innerHTML = $('#guide').innerHTML; };
  list.appendChild(guide);
  for (const [grp, t] of [[1, 'PaperS3'], [0, 'uLisp']]) {
    const items = docs.filter(d => d[2] === grp && (!q || d[0].includes(q) || d[1].toLowerCase().includes(q)));
    if (!items.length) continue;
    const g = document.createElement('div'); g.className = 'grp'; g.textContent = `${t} (${items.length})`; list.appendChild(g);
    for (const d of items.sort((a, b) => a[0].localeCompare(b[0]))) {
      const a = document.createElement('a'); a.textContent = d[0]; a.href = '#';
      a.onclick = e => { e.preventDefault(); showDoc(d); };
      list.appendChild(a);
    }
  }
  if (q) showResults(q);
}
function entryHtml(d) {
  const nl = d[1].indexOf('\n');
  const sig = nl < 0 ? d[1] : d[1].slice(0, nl), txt = nl < 0 ? '' : d[1].slice(nl + 1);
  return `<div class="entry"><div class="sig">${esc(sig)}${d[2] ? '<span class="tag">PaperS3</span>' : ''}</div><div class="txt">${esc(txt)}</div></div>`;
}
function showDoc(d) { $('#docbody').innerHTML = `<h2 class="mono">${esc(d[0])}</h2>` + entryHtml(d); }
function showResults(q) {
  const hits = docs.filter(d => d[0].includes(q) || d[1].toLowerCase().includes(q))
                   .sort((a, b) => (b[0].includes(q) - a[0].includes(q)) || (b[2] - a[2]) || a[0].localeCompare(b[0])).slice(0, 60);
  $('#docbody').innerHTML = `<h2>Results for “${esc(q)}”</h2>` + (hits.map(entryHtml).join('') || '<p>No matches.</p>');
}
$('#docsearch').oninput = e => renderDocList(e.target.value.trim());

// ---- Examples ----
function buildExamples() {
  const grid = $('#exgrid');
  document.querySelectorAll('script.example').forEach(s => {
    const code = s.textContent.replace(/^\n+|\s+$/g, '');
    const c = document.createElement('div'); c.className = 'card';
    c.innerHTML = `<h4>${esc(s.dataset.title)}</h4><p>${esc(s.dataset.desc)}</p><pre>${esc(code)}</pre>
      <div class="acts"><button class="b primary">Run</button><button class="b">Open in editor</button></div><div class="res out mono"></div>`;
    const [run, open] = c.querySelectorAll('button'), res = c.querySelector('.res');
    run.onclick = async () => {
      if (busy) return;
      res.className = 'res out mono pending'; res.textContent = 'running…';
      try { const t = await evalLisp(code); res.className = 'res out mono' + (isErr(t) ? ' err' : ''); res.textContent = t; }
      catch (e) { res.className = 'res out mono err'; res.textContent = 'connection error: ' + e; }
    };
    open.onclick = () => {
      if (dirty && !confirm('Discard unsaved changes?')) return;
      ed.value = code; curFile = '';
      fname.value = '/' + s.dataset.title.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') + '.lisp';
      setDirty(true); parenCheck(); edout.textContent = ''; show('editor');
    };
    grid.appendChild(c);
  });
}

// Drag the bar between the output and the input to resize the input box
(() => {
  const sp = $('#splitter'), key = 'ulisp-repl-h';
  const clamp = h => Math.max(60, Math.min(window.innerHeight - 160, h));
  const saved = +localStorage.getItem(key);
  if (saved) rc.style.height = clamp(saved) + 'px';
  sp.addEventListener('pointerdown', e => {
    e.preventDefault(); sp.setPointerCapture(e.pointerId); sp.classList.add('drag');
    const y0 = e.clientY, h0 = rc.offsetHeight;
    const move = ev => { rc.style.height = clamp(h0 + (y0 - ev.clientY)) + 'px'; };
    const up = () => { sp.classList.remove('drag'); sp.removeEventListener('pointermove', move);
      localStorage.setItem(key, rc.offsetHeight); log.scrollTop = log.scrollHeight; };
    sp.addEventListener('pointermove', move);
    sp.addEventListener('pointerup', up, { once:true });
  });
  sp.addEventListener('dblclick', () => { rc.style.height = ''; localStorage.removeItem(key); });
})();

buildExamples();
parenCheck();
updStatus();
show(['repl', 'editor', 'docs', 'examples'].includes(location.hash.slice(1)) ? location.hash.slice(1) : 'repl');
</script></body></html>)HTML";
