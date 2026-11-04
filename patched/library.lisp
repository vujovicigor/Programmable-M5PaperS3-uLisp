; Lisp library built into the firmware (embedded by tools/mkproto.py).
; Everything here can be redefined at the REPL.

; ---- EPUB reader ------------------------------------------------------------
; (reader)               library of /sd/books, then read
; (reader "/sd/books/x.epub")  open a book directly
; While reading: tap the right side for the next page, the left third for the
; previous page, and the top strip for the menu.

(defvar *books-dir* "/sd/books")

; Every tap goes through here, so it can be redefined for testing.
(defun reader-touch () (wait-touch))

(defun drop (n lst)
  (loop (when (or (<= n 0) (null lst)) (return lst)) (setq lst (cdr lst)) (decf n)))

(defun ends-with (s suffix)
  (let ((n (length s)) (m (length suffix)))
    (and (>= n m) (string= (subseq s (- n m)) suffix))))

(defun reader-books ()
  (let ((books nil))
    (dolist (f (list-files *books-dir*))
      (when (and (ends-with f ".epub") (not (string= (subseq f 0 (min 2 (length f))) "._")))
        (push f books)))
    (reverse books)))

(defun ui-button (label x y w h)
  (draw-rect x y w h 0)
  (draw-ttf label (+ x (truncate w 2)) (+ y (truncate h 2) 8) 24 1))

; Shows a paged list of strings; returns the index tapped, or nil for Back.
(defun ui-pick (title items)
  (let ((start 0) (per 9) (n (length items)))
    (loop
      (with-batch
        (clear-screen)
        (draw-ttf title 270 70 34 1 1 0 480)
        (let ((y 120) (i 0))
          (dolist (it (drop start items))
            (when (< i per)
              (draw-ttf it 40 (+ y 52) 26 0 0 0 460)
              (draw-line 40 (+ y 78) 500 (+ y 78) (gray 12))
              (setq y (+ y 80))
              (incf i))))
        (when (> start 0) (ui-button "Prev" 20 880 150 60))
        (ui-button "Back" 195 880 150 60)
        (when (< (+ start per) n) (ui-button "Next" 370 880 150 60)))
      (let* ((p (reader-touch)) (x (first p)) (y (second p)))
        (cond ((>= y 870)
               (cond ((< x 180) (when (> start 0) (setq start (- start per))))
                     ((< x 360) (return nil))
                     (t (when (< (+ start per) n) (setq start (+ start per))))))
              ((>= y 120)
               (let ((i (+ start (truncate (- y 120) 80))))
                 (when (and (< i n) (< (- i start) per)) (return i)))))))))

(defun reader-library ()
  (set-rotation 0)
  (let ((books (reader-books)))
    (if (null books)
        (progn
          (with-batch
            (clear-screen)
            (draw-ttf "No books" 270 200 34 1 1)
            (draw-ttf (format nil "Put .epub files in ~a" *books-dir*) 270 260 22 1))
          (wait-touch 5000)
          nil)
        (let ((i (ui-pick "Library" (mapcar (lambda (b) (subseq b 0 (- (length b) 5))) books))))
          (when i (concatenate 'string *books-dir* "/" (nth i books)))))))

; Returns back, library or exit.
(defun reader-menu ()
  (let ((info (book-info))
        (items '("Back to book" "Contents" "Larger text" "Smaller text" "Change font" "Library" "Exit reader")))
    (with-batch
      (clear-screen)
      (draw-ttf (json-get info "title") 270 70 28 1 1 0 480)
      (draw-ttf (format nil "~a  ~a pt  battery ~a%" (json-get info "font") (json-get info "font-size") (battery)) 270 110 18 1 0 6 480)
      (let ((y 150))
        (dolist (it items)
          (ui-button it 70 y 400 76)
          (setq y (+ y 96)))))
    (loop
      (let* ((p (reader-touch)) (y (second p)) (i (truncate (- y 150) 96)))
        (when (and (>= y 150) (< i 7))
          (cond ((= i 0) (return 'back))
                ((= i 1)
                 (let* ((toc (book-toc)) (k (ui-pick "Contents" (mapcar car toc))))
                   (when k (book-goto (cdr (nth k toc))) (return 'shown))
                   (return 'back)))
                ((= i 2) (book-font-size (min 60 (+ (book-font-size) 2))) (return 'shown))
                ((= i 3) (book-font-size (max 12 (- (book-font-size) 2))) (return 'shown))
                ((= i 4)
                 (let* ((fams (book-fonts)) (k (ui-pick "Font" fams)))
                   (when k (book-font (nth k fams)) (return 'shown))
                   (return 'back)))
                ((= i 5) (return 'library))
                (t (return 'exit))))))))

(defun reader-read (path)
  (book-open path)
  (loop
    (let* ((p (reader-touch)) (x (first p)) (y (second p)))
      (cond ((< y 110)
             (let ((r (reader-menu)))
               (cond ((eq r 'back) (book-render))
                     ((eq r 'library) (return 'library))
                     ((eq r 'exit) (return 'exit)))))
            ((< x 180) (book-prev))
            (t (book-next))))))

(defun reader (&optional path)
  (let ((next (if path (reader-read path) 'library)))
    (loop
      (unless (eq next 'library) (return))
      (let ((b (reader-library)))
        (if b (setq next (reader-read b)) (return)))))
  (book-close)
  (epd-mode 1)
  nil)
