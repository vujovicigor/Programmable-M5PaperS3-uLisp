// ---------------------------------------------------------------------------
// EPUB reader for the PaperS3: zip, OPF/NCX/nav parsing, XHTML to text,
// FreeType glyph cache on 16 grays, layout with justification, images,
// position saving. The touch interface is Lisp, in patched/library.lisp.
// ---------------------------------------------------------------------------

#include <vector>
#include <unordered_map>
#include "miniz.h"   // ROM inflate (tinfl)
#include <ft2build.h>
#include FT_FREETYPE_H

#define RD_W 540
#define RD_H 960
#define RD_MX 38          // left/right margin
#define RD_TOP 64         // first text line starts below this
#define RD_BOTTOM 902     // text must end above this
#define RD_UISIZE 18

void *rdmalloc (size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

// ---- Zip ------------------------------------------------------------------------

struct RdZipEntry { String name; uint32_t local, csize, usize; uint16_t method; };

File RdZipFile;
std::vector<RdZipEntry> RdZip;

uint16_t rd16 (const uint8_t *p) { return p[0] | (p[1] << 8); }
uint32_t rd32 (const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint32_t rd32be (const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

bool rdzipopen (fs::FS *fs, const String &sub) {
  RdZip.clear();
  if (RdZipFile) RdZipFile.close();
  RdZipFile = fs->open(sub, "r");
  if (!RdZipFile) return false;
  size_t size = RdZipFile.size();
  size_t tail = size < 65557 ? size : 65557;
  uint8_t *buf = (uint8_t *)rdmalloc(tail);
  if (!buf) return false;
  RdZipFile.seek(size - tail);
  RdZipFile.read(buf, tail);
  long eocd = -1;
  for (long i = (long)tail - 22; i >= 0; i--) if (rd32(buf + i) == 0x06054b50) { eocd = i; break; }
  if (eocd < 0) { free(buf); return false; }
  uint16_t count = rd16(buf + eocd + 10);
  uint32_t cdsize = rd32(buf + eocd + 12), cdoff = rd32(buf + eocd + 16);
  free(buf);
  uint8_t *cd = (uint8_t *)rdmalloc(cdsize);
  if (!cd) return false;
  RdZipFile.seek(cdoff);
  RdZipFile.read(cd, cdsize);
  size_t p = 0;
  for (int i = 0; i < count && p + 46 <= cdsize; i++) {
    if (rd32(cd + p) != 0x02014b50) break;
    RdZipEntry e;
    e.method = rd16(cd + p + 10);
    e.csize = rd32(cd + p + 20); e.usize = rd32(cd + p + 24);
    uint16_t nlen = rd16(cd + p + 28), xlen = rd16(cd + p + 30), clen = rd16(cd + p + 32);
    e.local = rd32(cd + p + 42);
    e.name = String((const char *)(cd + p + 46), nlen);
    RdZip.push_back(e);
    p += 46 + nlen + xlen + clen;
  }
  free(cd);
  return !RdZip.empty();
}

int rdzipfind (const String &name) {
  for (size_t i = 0; i < RdZip.size(); i++) if (RdZip[i].name == name) return i;
  for (size_t i = 0; i < RdZip.size(); i++) if (RdZip[i].name.equalsIgnoreCase(name)) return i;
  return -1;
}

// Returns a malloc'd, NUL-terminated buffer with the entry's contents
uint8_t *rdzipread (int idx, size_t *lenout) {
  if (idx < 0 || idx >= (int)RdZip.size()) return NULL;
  RdZipEntry &e = RdZip[idx];
  uint8_t hdr[30];
  RdZipFile.seek(e.local);
  if (RdZipFile.read(hdr, 30) != 30 || rd32(hdr) != 0x04034b50) return NULL;
  RdZipFile.seek(e.local + 30 + rd16(hdr + 26) + rd16(hdr + 28));
  uint8_t *out = (uint8_t *)rdmalloc(e.usize + 1);
  if (!out) return NULL;
  if (e.method == 0) {
    RdZipFile.read(out, e.usize);
  } else if (e.method == 8) {
    uint8_t *in = (uint8_t *)rdmalloc(e.csize);
    if (!in) { free(out); return NULL; }
    RdZipFile.read(in, e.csize);
    tinfl_decompressor *d = (tinfl_decompressor *)rdmalloc(sizeof(tinfl_decompressor));
    if (!d) { free(in); free(out); return NULL; }
    tinfl_init(d);
    size_t inlen = e.csize, outlen = e.usize;
    tinfl_status st = tinfl_decompress(d, in, &inlen, out, out, &outlen, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(d); free(in);
    if (st < 0) { free(out); return NULL; }
  } else { free(out); return NULL; }
  out[e.usize] = 0;
  if (lenout) *lenout = e.usize;
  return out;
}

// ---- Small XML helpers ------------------------------------------------------------

// Finds the next tag at or after pos. name is lowercased without namespace prefix.
bool rdnexttag (const char *s, size_t n, size_t &pos, String &name, String &attrs, bool &closing, bool &selfclose) {
  while (pos < n) {
    const char *lt = (const char *)memchr(s + pos, '<', n - pos);
    if (!lt) { pos = n; return false; }
    size_t i = lt - s + 1;
    if (i + 2 < n && s[i] == '!' && s[i+1] == '-' && s[i+2] == '-') {   // comment
      const char *end = strstr(s + i, "-->");
      pos = end ? end - s + 3 : n; continue;
    }
    if (i < n && (s[i] == '!' || s[i] == '?')) {
      const char *end = (const char *)memchr(s + i, '>', n - i);
      pos = end ? end - s + 1 : n; continue;
    }
    closing = (i < n && s[i] == '/'); if (closing) i++;
    size_t ns = i;
    while (i < n && !isspace((unsigned char)s[i]) && s[i] != '>' && s[i] != '/') i++;
    name = String(s + ns, i - ns); name.toLowerCase();
    int colon = name.indexOf(':'); if (colon >= 0) name = name.substring(colon + 1);
    size_t as = i;
    char q = 0;
    while (i < n) { char c = s[i]; if (q) { if (c == q) q = 0; } else if (c == '"' || c == '\'') q = c; else if (c == '>') break; i++; }
    size_t ae = i;
    selfclose = (ae > as && s[ae-1] == '/');
    attrs = String(s + as, (selfclose ? ae - 1 : ae) - as);
    pos = i < n ? i + 1 : n;
    return true;
  }
  return false;
}

String rdattr (const String &attrs, const char *key) {
  int klen = strlen(key);
  int i = 0;
  while ((i = attrs.indexOf(key, i)) >= 0) {
    bool startok = i == 0 || isspace((unsigned char)attrs[i-1]) || attrs[i-1] == ':';
    int j = i + klen;
    while (j < (int)attrs.length() && attrs[j] == ' ') j++;
    if (startok && j < (int)attrs.length() && attrs[j] == '=') {
      j++;
      while (j < (int)attrs.length() && attrs[j] == ' ') j++;
      char q = attrs[j];
      if (q == '"' || q == '\'') { int e = attrs.indexOf(q, j + 1); if (e > j) return attrs.substring(j + 1, e); }
    }
    i += klen;
  }
  return "";
}

void rdutf8put (String &out, uint32_t cp) {
  if (cp < 0x80) out += (char)cp;
  else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
  else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

struct RdEntity { const char *name; uint16_t cp; };
const RdEntity RdEntities[] = {
  {"amp",'&'},{"lt",'<'},{"gt",'>'},{"quot",'"'},{"apos",'\''},{"nbsp",0xA0},{"mdash",0x2014},{"ndash",0x2013},
  {"hellip",0x2026},{"lsquo",0x2018},{"rsquo",0x2019},{"ldquo",0x201C},{"rdquo",0x201D},{"laquo",0xAB},{"raquo",0xBB},
  {"copy",0xA9},{"reg",0xAE},{"trade",0x2122},{"shy",0xAD},{"bull",0x2022},{"middot",0xB7},{"deg",0xB0},{"times",0xD7},
  {"eacute",0xE9},{"egrave",0xE8},{"agrave",0xE0},{"ccedil",0xE7},{"ouml",0xF6},{"uuml",0xFC},{"auml",0xE4},{"szlig",0xDF},
  {"thinsp",0x2009},{"ensp",0x2002},{"emsp",0x2003},{"zwnj",0x200C},{"zwj",0x200D},{"para",0xB6},{"sect",0xA7},
};

// Decodes the entity starting at s[i] == '&'; returns chars consumed (0 if not an entity)
int rdentity (const char *s, size_t n, size_t i, String &out) {
  size_t j = i + 1, e = j;
  while (e < n && e - j < 10 && s[e] != ';' && s[e] != '&' && !isspace((unsigned char)s[e]) && s[e] != '<') e++;
  if (e >= n || s[e] != ';') return 0;
  String nm(s + j, e - j);
  uint32_t cp = 0;
  if (nm.length() > 1 && nm[0] == '#') {
    if (nm[1] == 'x' || nm[1] == 'X') cp = strtoul(nm.c_str() + 2, NULL, 16); else cp = strtoul(nm.c_str() + 1, NULL, 10);
  } else {
    for (auto &en : RdEntities) if (nm == en.name) { cp = en.cp; break; }
  }
  if (!cp) return 0;
  if (cp == 0xAD || cp == 0x200C || cp == 0x200D) return e - i + 1;   // soft hyphen etc: drop
  rdutf8put(out, cp);
  return e - i + 1;
}

String rdtext (const String &s) {   // plain text of an XML fragment, entities decoded, spaces collapsed
  String out; bool sp = false; bool intag = false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (intag) { if (c == '>') intag = false; continue; }
    if (c == '<') { intag = true; continue; }
    if (isspace((unsigned char)c)) { sp = out.length() > 0; continue; }
    if (sp) { out += ' '; sp = false; }
    if (c == '&') { int k = rdentity(s.c_str(), s.length(), i, out); if (k) { i += k - 1; continue; } }
    out += c;
  }
  return out;
}

String rdurldecode (const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == '%' && i + 2 < s.length() && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])) {
      char h[3] = { s[i+1], s[i+2], 0 }; out += (char)strtol(h, NULL, 16); i += 2;
    } else out += s[i];
  }
  return out;
}

// Resolves href relative to dir ("OEBPS/text/"), handling ../ and ./, dropping #fragment
String rdresolve (const String &dir, String href) {
  int hash = href.indexOf('#'); if (hash >= 0) href = href.substring(0, hash);
  href = rdurldecode(href);
  String path = href.startsWith("/") ? href.substring(1) : dir + href;
  std::vector<String> parts;
  int start = 0;
  while (start <= (int)path.length()) {
    int sl = path.indexOf('/', start); if (sl < 0) sl = path.length();
    String part = path.substring(start, sl);
    if (part == "..") { if (!parts.empty()) parts.pop_back(); }
    else if (part.length() && part != ".") parts.push_back(part);
    start = sl + 1;
  }
  String out;
  for (size_t i = 0; i < parts.size(); i++) { if (i) out += '/'; out += parts[i]; }
  return out;
}

String rddirof (const String &path) { int sl = path.lastIndexOf('/'); return sl >= 0 ? path.substring(0, sl + 1) : String(""); }

// ---- Fonts and glyphs ---------------------------------------------------------------

struct RdGlyph { int16_t left, top; uint16_t w, h; int16_t adv; uint8_t *bmp; };  // bmp: 1 byte/px, 0-15

FT_Library RdFT = NULL;
FT_Face RdFace[4] = { NULL, NULL, NULL, NULL };   // regular, bold, italic, bold italic
uint8_t *RdFontData[4] = { NULL, NULL, NULL, NULL };
String RdFamily;
std::unordered_map<uint64_t, RdGlyph> RdGlyphs;

void rdclearglyphs () {
  for (auto &kv : RdGlyphs) free(kv.second.bmp);
  RdGlyphs.clear();
}

void rdunloadfonts () {
  rdclearglyphs();
  for (int i = 0; i < 4; i++) {
    if (RdFace[i]) FT_Done_Face(RdFace[i]);
    RdFace[i] = NULL;
    free(RdFontData[i]); RdFontData[i] = NULL;
  }
  RdFamily = "";
}

// Lists font files (".ttf"/".otf") in /sd/fonts and /fonts
void rdfontfiles (std::vector<String> &out) {
  const char *dirs[] = { "/sd/fonts", "/fonts" };
  for (const char *d : dirs) {
    String sub;
    fs::FS *fs = resolvepath(d, sub);
    if (!fs) continue;
    File dir = fs->open(sub);
    if (!dir || !dir.isDirectory()) continue;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String nm = f.name(); String lower = nm; lower.toLowerCase();
      if (!f.isDirectory() && !nm.startsWith("._") && (lower.endsWith(".ttf") || lower.endsWith(".otf")))
        out.push_back(String(d) + "/" + nm);
      f.close();
    }
    dir.close();
  }
}

String rdbasename (const String &p) { int sl = p.lastIndexOf('/'); String b = p.substring(sl + 1); int dot = b.lastIndexOf('.'); return dot > 0 ? b.substring(0, dot) : b; }

bool rdloadface (int slot, const String &path) {
  String sub;
  fs::FS *fs = resolvepath(path.c_str(), sub);
  if (!fs) return false;
  File f = fs->open(sub, "r");
  if (!f) return false;
  size_t n = f.size();
  uint8_t *data = (uint8_t *)rdmalloc(n);
  if (!data) { f.close(); return false; }
  f.read(data, n); f.close();
  FT_Face face;
  if (FT_New_Memory_Face(RdFT, data, n, 0, &face)) { free(data); return false; }
  RdFace[slot] = face; RdFontData[slot] = data;
  return true;
}

// Loads a family by name prefix, e.g. "CrimsonPro". Returns false if no regular face was found.
bool rdloadfamily (const String &family) {
  if (!RdFT && FT_Init_FreeType(&RdFT)) return false;
  std::vector<String> files;
  rdfontfiles(files);
  const char *suffix[4][3] = { {"-Regular", "", "-Book"}, {"-Bold", "-Semibold", "-SemiBold"},
                               {"-Italic", "-Oblique", ""}, {"-BoldItalic", "-BoldOblique", ""} };
  String chosen[4];
  for (int s = 0; s < 4; s++)
    for (int k = 0; k < 3 && !chosen[s].length(); k++) {
      if (s > 0 && !suffix[s][k][0]) continue;
      String want = family + suffix[s][k];
      for (auto &p : files) if (rdbasename(p).equalsIgnoreCase(want)) { chosen[s] = p; break; }
    }
  if (!chosen[0].length()) return false;
  rdunloadfonts();
  for (int s = 0; s < 4; s++) if (chosen[s].length()) rdloadface(s, chosen[s]);
  if (!RdFace[0]) return false;
  RdFamily = family;
  return true;
}

bool rdensurefonts () {
  if (RdFace[0]) return true;
  const char *prefs[] = { "CrimsonPro", "Caladea", "DejaVuSerif", "Bitter", "OpenSans", "Roboto", "DejaVuSans" };
  for (const char *p : prefs) if (rdloadfamily(p)) return true;
  std::vector<String> files;          // any font with a -Regular face
  rdfontfiles(files);
  for (auto &p : files) { String b = rdbasename(p); int r = b.indexOf("-Regular"); if (r > 0 && rdloadfamily(b.substring(0, r))) return true; }
  for (auto &p : files) if (rdloadfamily(rdbasename(p))) return true;
  return false;
}

FT_Face rdface (int style) {
  style &= 3;
  if (RdFace[style]) return RdFace[style];
  if (style == 3 && RdFace[1]) return RdFace[1];
  return RdFace[0];
}

RdGlyph *rdglyph (uint32_t cp, int style, int size) {
  uint64_t key = ((uint64_t)(style & 3) << 40) | ((uint64_t)size << 24) | cp;
  auto it = RdGlyphs.find(key);
  if (it != RdGlyphs.end()) return &it->second;
  if (RdGlyphs.size() > 4000) rdclearglyphs();
  FT_Face face = rdface(style);
  RdGlyph g = { 0, 0, 0, 0, 0, NULL };
  if (face) {
    FT_Set_Pixel_Sizes(face, 0, size);
    FT_UInt gi = FT_Get_Char_Index(face, cp);
    if (gi == 0 && face != RdFace[0]) { face = RdFace[0]; FT_Set_Pixel_Sizes(face, 0, size); gi = FT_Get_Char_Index(face, cp); }
    if (gi == 0 && cp == 0xA0) gi = FT_Get_Char_Index(face, ' ');
    if (gi == 0) gi = FT_Get_Char_Index(face, '?');
    if (FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT) == 0 && FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) == 0) {
      FT_Bitmap &b = face->glyph->bitmap;
      g.left = face->glyph->bitmap_left; g.top = face->glyph->bitmap_top;
      g.w = b.width; g.h = b.rows;
      g.adv = (face->glyph->advance.x + 32) >> 6;
      if (g.w && g.h) {
        g.bmp = (uint8_t *)rdmalloc(g.w * g.h);
        if (g.bmp) for (int y = 0; y < g.h; y++) for (int x = 0; x < g.w; x++) g.bmp[y * g.w + x] = b.buffer[y * b.pitch + x] >> 4;
        else { g.w = g.h = 0; }
      }
    }
  }
  return &(RdGlyphs[key] = g);
}

uint32_t rdnextcp (const char *s, int len, int &i) {
  uint8_t c = s[i++];
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  uint32_t cp = c & (0x3F >> extra);
  while (extra-- > 0 && i < len) cp = (cp << 6) | (s[i++] & 0x3F);
  return cp;
}

int rdtextwidth (const char *s, int len, int style, int size) {
  int w = 0, i = 0;
  while (i < len) w += rdglyph(rdnextcp(s, len, i), style, size)->adv;
  return w;
}

int rdascent (int style, int size) {
  FT_Face f = rdface(style);
  if (!f) return size;
  FT_Set_Pixel_Sizes(f, 0, size);
  return (f->size->metrics.ascender + 63) >> 6;
}

uint16_t RdInk[16];   // gray levels: 0 = white paper ... 15 = black ink
int RdTextInk = 15;   // darkness of text being drawn, 0-15

void rdinitink () { for (int v = 0; v < 16; v++) { uint8_t g = 255 - v * 17; RdInk[v] = tft.color565(g, g, g); } }

// Draws UTF-8 text with its baseline at y; returns the advance
int rddrawtext (const char *s, int len, int x, int y, int style, int size) {
  int i = 0, x0 = x;
  while (i < len) {
    RdGlyph *g = rdglyph(rdnextcp(s, len, i), style, size);
    if (g->bmp) {
      int gx = x + g->left, gy = y - g->top;
      for (int r = 0; r < g->h; r++) {
        const uint8_t *row = g->bmp + r * g->w;
        int c = 0;
        while (c < g->w) {
          uint8_t v = row[c];
          if (!v) { c++; continue; }
          int run = 1;
          while (c + run < g->w && row[c + run] == v) run++;
          tft.drawFastHLine(gx + c, gy + r, run, RdInk[(v * RdTextInk + 7) / 15]);
          c += run;
        }
      }
    }
    x += g->adv;
  }
  return x - x0;
}

// ---- Book structure -------------------------------------------------------------

struct RdTocEntry { String title; int chapter; uint8_t depth; };

String RdBookPath, RdTitle, RdAuthor;
std::vector<String> RdSpine;
std::vector<RdTocEntry> RdToc;
bool RdOpen = false;

bool rdloadstructure () {
  RdSpine.clear(); RdToc.clear(); RdTitle = ""; RdAuthor = "";
  size_t n;
  char *container = (char *)rdzipread(rdzipfind("META-INF/container.xml"), &n);
  if (!container) return false;
  String opfpath;
  { size_t pos = 0; String nm, at; bool cl, sc;
    while (rdnexttag(container, n, pos, nm, at, cl, sc)) if (!cl && nm == "rootfile") { opfpath = rdurldecode(rdattr(at, "full-path")); break; } }
  free(container);
  if (!opfpath.length()) return false;
  char *opf = (char *)rdzipread(rdzipfind(opfpath), &n);
  if (!opf) return false;
  String opfdir = rddirof(opfpath);
  struct Item { String id, href, type, props; };
  std::vector<Item> manifest;
  std::vector<String> spineids;
  String tocid;
  size_t pos = 0; String nm, at; bool cl, sc;
  while (rdnexttag(opf, n, pos, nm, at, cl, sc)) {
    if (cl) continue;
    if ((nm == "title" && !RdTitle.length()) || (nm == "creator" && !RdAuthor.length())) {
      const char *end = strstr(opf + pos, "</");
      if (end) { String t = rdtext(String(opf + pos, end - (opf + pos))); if (nm == "title") RdTitle = t; else RdAuthor = t; }
    } else if (nm == "item") {
      manifest.push_back({ rdattr(at, "id"), rdattr(at, "href"), rdattr(at, "media-type"), rdattr(at, "properties") });
    } else if (nm == "spine") {
      tocid = rdattr(at, "toc");
    } else if (nm == "itemref") {
      spineids.push_back(rdattr(at, "idref"));
    }
  }
  free(opf);
  for (auto &id : spineids)
    for (auto &it : manifest) if (it.id == id) { RdSpine.push_back(rdresolve(opfdir, it.href)); break; }
  if (RdSpine.empty()) return false;

  // Table of contents: EPUB 3 nav document, else EPUB 2 NCX
  String navpath, ncxpath;
  for (auto &it : manifest) {
    if (it.props.indexOf("nav") >= 0) navpath = rdresolve(opfdir, it.href);
    if (it.id == tocid || it.type == "application/x-dtbncx+xml") ncxpath = rdresolve(opfdir, it.href);
  }
  auto chapterof = [](const String &path) { for (size_t i = 0; i < RdSpine.size(); i++) if (RdSpine[i] == path) return (int)i; return -1; };
  if (ncxpath.length()) {
    char *ncx = (char *)rdzipread(rdzipfind(ncxpath), &n);
    if (ncx) {
      String dir = rddirof(ncxpath), label;
      int depth = 0; bool inlabel = false;
      pos = 0;
      while (rdnexttag(ncx, n, pos, nm, at, cl, sc)) {
        if (nm == "navpoint") depth += cl ? -1 : 1;
        else if (nm == "text" && !cl && depth > 0) {
          const char *end = strstr(ncx + pos, "</"); if (end) label = rdtext(String(ncx + pos, end - (ncx + pos)));
          inlabel = true;
        } else if (nm == "content" && !cl && inlabel) {
          int ch = chapterof(rdresolve(dir, rdattr(at, "src")));
          if (ch >= 0) RdToc.push_back({ label, ch, (uint8_t)(depth > 0 ? depth - 1 : 0) });
          inlabel = false;
        }
      }
      free(ncx);
    }
  }
  if (RdToc.empty() && navpath.length()) {
    char *nav = (char *)rdzipread(rdzipfind(navpath), &n);
    if (nav) {
      String dir = rddirof(navpath);
      bool intoc = false; int depth = 0;
      pos = 0;
      while (rdnexttag(nav, n, pos, nm, at, cl, sc)) {
        if (nm == "nav") { if (!cl && rdattr(at, "type") == "toc") intoc = true; else if (cl && intoc) break; }
        else if (intoc && nm == "ol") depth += cl ? -1 : 1;
        else if (intoc && nm == "a" && !cl) {
          const char *end = strstr(nav + pos, "</a>");
          String label = end ? rdtext(String(nav + pos, end - (nav + pos))) : String("");
          int ch = chapterof(rdresolve(dir, rdattr(at, "href")));
          if (ch >= 0 && label.length()) RdToc.push_back({ label, ch, (uint8_t)(depth > 0 ? depth - 1 : 0) });
        }
      }
      free(nav);
    }
  }
  if (!RdTitle.length()) RdTitle = rdbasename(RdBookPath);
  return true;
}

// ---- Chapter content: XHTML to a flat list of items -------------------------------

enum : uint8_t { RK_WORD, RK_PARA, RK_BR, RK_IMAGE, RK_HR };
enum : uint8_t { RB_P, RB_H1, RB_H2, RB_H3, RB_H4, RB_H5, RB_H6, RB_PRE, RB_LI, RB_QUOTE, RB_CENTER };

struct RdItem { uint32_t text; uint16_t len; uint8_t kind, style, block, space; int16_t w; };

std::vector<RdItem> RdItems;
char *RdPool = NULL;
size_t RdPoolLen = 0, RdPoolCap = 0;

uint32_t rdpooladd (const char *s, size_t n) {
  if (RdPoolLen + n + 1 > RdPoolCap) {
    size_t cap = RdPoolCap ? RdPoolCap * 2 : 65536;
    while (cap < RdPoolLen + n + 1) cap *= 2;
    char *p = (char *)heap_caps_realloc(RdPool, cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) error2("out of memory reading book");
    RdPool = p; RdPoolCap = cap;
  }
  uint32_t off = RdPoolLen;
  memcpy(RdPool + off, s, n); RdPool[off + n] = 0;
  RdPoolLen += n + 1;
  return off;
}

void rdclearchapter () { RdItems.clear(); RdItems.shrink_to_fit(); RdPoolLen = 0; }

struct RdParser {
  String word; bool pendingspace = false;
  int bold = 0, italic = 0, pre = 0, skip = 0;
  std::vector<uint8_t> blocks;
  std::vector<int> olcount;
  uint8_t block () { return blocks.empty() ? RB_P : blocks.back(); }
  uint8_t style () { uint8_t b = block(); return ((bold || (b >= RB_H1 && b <= RB_H6)) ? 1 : 0) | (italic ? 2 : 0); }
  void flush () {
    if (!word.length()) return;
    RdItem it = { rdpooladd(word.c_str(), word.length()), (uint16_t)word.length(), RK_WORD, style(), block(), (uint8_t)pendingspace, 0 };
    RdItems.push_back(it);
    word = ""; pendingspace = false;
  }
  void addmark (uint8_t kind) {
    flush();
    if (kind == RK_PARA && (RdItems.empty() || RdItems.back().kind == RK_PARA)) return;
    RdItems.push_back({ 0, 0, kind, 0, block(), 0, 0 });
    pendingspace = false;
  }
  void wordchar (const char *c, int n) { word.concat(c, n); }
};

bool rdisblock (const String &t) {
  static const char *b[] = { "p", "div", "section", "article", "header", "footer", "aside", "figure", "figcaption",
    "ul", "ol", "dl", "dt", "dd", "table", "tr", "caption", "body", "h1", "h2", "h3", "h4", "h5", "h6", "li", "blockquote", "pre", "center", "nav" };
  for (auto x : b) if (t == x) return true;
  return false;
}

void rdparsechapter (const char *s, size_t n, const String &dir) {
  rdclearchapter();
  RdParser P;
  size_t pos = 0;
  const char *body = strstr(s, "<body");
  if (body) pos = body - s;
  while (pos < n) {
    char c = s[pos];
    if (c == '<') {
      String nm, at; bool cl, sc;
      size_t tagstart = pos;
      if (!rdnexttag(s, n, pos, nm, at, cl, sc)) break;
      if (pos <= tagstart) break;
      if (nm == "head" || nm == "style" || nm == "script" || nm == "title") { if (!sc) P.skip += cl ? -1 : 1; if (P.skip < 0) P.skip = 0; continue; }
      if (P.skip) continue;
      if (nm == "b" || nm == "strong" || nm == "th") { if (!sc) { P.flush(); P.bold += cl ? -1 : 1; if (P.bold < 0) P.bold = 0; } }
      else if (nm == "i" || nm == "em" || nm == "cite" || nm == "var" || nm == "dfn") { if (!sc) { P.flush(); P.italic += cl ? -1 : 1; if (P.italic < 0) P.italic = 0; } }
      else if (nm == "br") { P.addmark(RK_BR); }
      else if (nm == "hr") { P.addmark(RK_PARA); P.addmark(RK_HR); P.addmark(RK_PARA); }
      else if (nm == "img" || nm == "image") {
        String src = rdattr(at, nm == "img" ? "src" : "href");
        if (src.length()) {
          P.addmark(RK_PARA);
          String path = rdresolve(dir, src);
          RdItems.push_back({ rdpooladd(path.c_str(), path.length()), (uint16_t)path.length(), RK_IMAGE, 0, P.block(), 0, 0 });
          P.addmark(RK_PARA);
        }
      }
      else if (nm == "td") { P.flush(); P.pendingspace = true; }
      else if (rdisblock(nm)) {
        P.addmark(RK_PARA);
        if (sc) continue;
        uint8_t b = 255;
        if (nm.length() == 2 && nm[0] == 'h' && nm[1] >= '1' && nm[1] <= '6') b = RB_H1 + (nm[1] - '1');
        else if (nm == "pre") b = RB_PRE;
        else if (nm == "li") b = RB_LI;
        else if (nm == "blockquote" || nm == "dd") b = RB_QUOTE;
        else if (nm == "center") b = RB_CENTER;
        if (nm == "ol") { if (cl) { if (!P.olcount.empty()) P.olcount.pop_back(); } else P.olcount.push_back(0); }
        if (nm == "ul") { if (cl) { if (!P.olcount.empty()) P.olcount.pop_back(); } else P.olcount.push_back(-1); }
        if (b != 255) {
          if (cl) { if (!P.blocks.empty()) P.blocks.pop_back(); if (b == RB_PRE && P.pre) P.pre--; }
          else {
            P.blocks.push_back(b);
            if (b == RB_PRE) P.pre++;
            if (b == RB_LI) {
              String bullet = "\xE2\x80\xA2";
              if (!P.olcount.empty() && P.olcount.back() >= 0) bullet = String(++P.olcount.back()) + ".";
              P.word = bullet; P.flush(); P.pendingspace = true;
            }
          }
        }
      }
      continue;
    }
    pos++;
    if (P.skip) continue;
    if (c == '&') {
      String ent;
      int k = rdentity(s, n, pos - 1, ent);
      if (k) {
        pos += k - 1;
        if (ent == "\xC2\xA0" && !P.pre) { P.wordchar(ent.c_str(), ent.length()); continue; }   // nbsp keeps words together
        P.wordchar(ent.c_str(), ent.length());
        continue;
      }
    }
    if (P.pre) {
      if (c == '\n') { P.flush(); P.addmark(RK_BR); continue; }
      if (c == '\r') continue;
      if (c == ' ' || c == '\t') { P.flush(); P.pendingspace = true; if (c == '\t') { P.word = "   "; } continue; }
      P.wordchar(&c, 1);
      continue;
    }
    if (isspace((unsigned char)c)) { P.flush(); if (!RdItems.empty() && RdItems.back().kind == RK_WORD) P.pendingspace = true; continue; }
    P.wordchar(&c, 1);
  }
  P.addmark(RK_PARA);
}

// ---- Layout -------------------------------------------------------------------------

struct RdLine { uint32_t first, last; int16_t y, x; float gap; uint8_t size; uint8_t flags; int16_t imgh; };
enum : uint8_t { RL_JUSTIFY = 1, RL_IMAGE = 2, RL_HR = 4, RL_CENTER = 8, RL_CODE = 16 };

std::vector<RdLine> RdLines;
std::vector<uint32_t> RdPages;    // index of first line on each page
int RdFontSize = 26;
int RdChapter = -1, RdPage = 0;

int rdblocksize (uint8_t b) {
  switch (b) {
    case RB_H1: return RdFontSize * 16 / 10;
    case RB_H2: return RdFontSize * 14 / 10;
    case RB_H3: return RdFontSize * 12 / 10;
    case RB_H4: return RdFontSize * 11 / 10;
    case RB_PRE: return RdFontSize * 8 / 10;
    default: return RdFontSize;
  }
}

// Reads an image's size from its header without decoding it
bool rdimagesize (const uint8_t *d, size_t n, int &w, int &h) {
  if (n > 24 && d[0] == 0x89 && d[1] == 'P') { w = rd32be(d + 16); h = rd32be(d + 20); return true; }
  if (n > 4 && d[0] == 0xFF && d[1] == 0xD8) {
    size_t i = 2;
    while (i + 9 < n) {
      if (d[i] != 0xFF) { i++; continue; }
      uint8_t m = d[i+1];
      if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) { h = (d[i+5] << 8) | d[i+6]; w = (d[i+7] << 8) | d[i+8]; return true; }
      i += 2 + ((d[i+2] << 8) | d[i+3]);
    }
  }
  return false;
}

void rdlayout () {
  RdLines.clear(); RdPages.clear();
  int y = RD_TOP;
  RdPages.push_back(0);
  auto newline = [&](int h) { if (y + h > RD_BOTTOM && !RdLines.empty() && RdPages.back() != RdLines.size()) { RdPages.push_back(RdLines.size()); y = RD_TOP; } };
  size_t i = 0, N = RdItems.size();
  bool afterheading = true;
  while (i < N) {
    RdItem &it = RdItems[i];
    if (it.kind == RK_PARA || it.kind == RK_BR) { i++; continue; }
    if (it.kind == RK_HR) {
      newline(RdFontSize);
      RdLines.push_back({ (uint32_t)i, (uint32_t)i, (int16_t)(y + RdFontSize / 2), RD_MX, 0, (uint8_t)RdFontSize, RL_HR, 0 });
      y += RdFontSize; i++; continue;
    }
    if (it.kind == RK_IMAGE) {
      size_t n; int w = 0, h = 0;
      uint8_t *data = rdzipread(rdzipfind(String(RdPool + it.text)), &n);
      if (data) { rdimagesize(data, n, w, h); free(data); }
      if (w > 0 && h > 0) {
        float scale = 1.0f;
        int maxw = RD_W - 2 * RD_MX, maxh = RD_BOTTOM - RD_TOP;
        if (w > maxw) scale = (float)maxw / w;
        if (h * scale > maxh) scale = (float)maxh / h;
        if (w < 64 && h < 64 && scale >= 1.0f) scale = 1.0f;   // small images stay small
        int ih = h * scale + 0.5f;
        newline(ih);
        RdLines.push_back({ (uint32_t)i, (uint32_t)i, (int16_t)y, (int16_t)((RD_W - (int)(w * scale)) / 2), scale, 0, RL_IMAGE, (int16_t)ih });
        y += ih + RdFontSize / 3;
      }
      i++; continue;
    }
    // A paragraph: words up to the next RK_PARA. RK_BR forces a line break.
    uint8_t b = it.block;
    int size = rdblocksize(b);
    bool heading = b >= RB_H1 && b <= RB_H6;
    int indentleft = (b == RB_LI || b == RB_QUOTE) ? size * 3 / 2 : b == RB_PRE ? RdFontSize : 0;
    int avail = RD_W - 2 * RD_MX - indentleft;
    int firstindent = (b == RB_P && !afterheading) ? size * 3 / 2 : 0;
    int lh = b == RB_PRE ? size * 13 / 10 : size * 14 / 10;
    if (heading) { y += size / 2; newline(lh * 2); }
    int spacew = rdglyph(' ', 0, size)->adv;
    bool firstline = true;
    while (i < N && RdItems[i].kind != RK_PARA && RdItems[i].kind != RK_IMAGE && RdItems[i].kind != RK_HR) {
      if (RdItems[i].kind == RK_BR) { i++; if (!firstline) {} continue; }
      int x0 = firstline ? firstindent : 0;
      int width = 0, words = 0;
      size_t start = i;
      bool broke = false;
      while (i < N && RdItems[i].kind == RK_WORD) {
        RdItem &w = RdItems[i];
        int ws = rdblocksize(w.block);
        w.w = rdtextwidth(RdPool + w.text, w.len, w.style, ws);
        int gap = (words > 0 && w.space) ? spacew : 0;
        if (words > 0 && x0 + width + gap + w.w > avail) { broke = true; break; }
        if (words == 0 && w.w > avail) w.w = avail;   // very long word: let it overflow into the margin
        width += gap + w.w; words++; i++;
      }
      bool hardbreak = i < N && RdItems[i].kind == RK_BR;
      newline(lh);
      int gaps = 0;
      for (size_t k = start + 1; k < i; k++) if (RdItems[k].space) gaps++;
      float gap = spacew;
      uint8_t flags = b == RB_PRE ? RL_CODE : 0;
      if (broke && gaps > 0 && !heading && b != RB_PRE && b != RB_CENTER) {
        gap = spacew + (float)(avail - x0 - width) / gaps;   // always justify, as e-readers without hyphenation do
        flags |= RL_JUSTIFY;
      }
      int lx = RD_MX + indentleft + x0;
      if (b == RB_CENTER || b == RB_H1) { lx = RD_MX + (avail - width) / 2; flags |= RL_CENTER; }
      if (i > start) RdLines.push_back({ (uint32_t)start, (uint32_t)(i - 1), (int16_t)y, (int16_t)lx, gap, (uint8_t)size, flags, 0 });
      y += lh;
      firstline = false;
      if (hardbreak) i++;
      if (i == start) i++;   // safety: always advance
    }
    y += heading ? size / 3 : (b == RB_P ? lh / 6 : lh / 3);
    afterheading = heading;
  }
}

// ---- Rendering ------------------------------------------------------------------------

int RdTurns = 0;

void rdui (const String &s, int x, int y, int size, int align, int style = 0, uint8_t ink = 15) {
  int w = rdtextwidth(s.c_str(), s.length(), style, size);
  if (align == 1) x -= w / 2; else if (align == 2) x -= w;
  RdTextInk = ink;
  rddrawtext(s.c_str(), s.length(), x, y, style, size);
  RdTextInk = 15;
}

String rdellipsize (const String &s, int maxw, int size, int style = 0) {
  if (rdtextwidth(s.c_str(), s.length(), style, size) <= maxw) return s;
  String t = s;
  while (t.length() > 1 && rdtextwidth(t.c_str(), t.length(), style, size) > maxw - 20) {
    int k = t.length() - 1;
    while (k > 0 && (t[k] & 0xC0) == 0x80) k--;
    t = t.substring(0, k);
  }
  return t + "\xE2\x80\xA6";
}

void rdrender () {
  if (RdChapter < 0) return;
  RdTurns++;
  tft.setEpdMode(RdTurns % 10 == 0 ? epd_mode_t::epd_quality : epd_mode_t::epd_text);
  tft.startWrite();
  tft.fillScreen(TFT_WHITE);
  rdui(rdellipsize(RdTitle, RD_W - 2 * RD_MX, RD_UISIZE, 2), RD_W / 2, 36, RD_UISIZE, 1, 2, 10);
  uint32_t first = RdPages.empty() ? 0 : RdPages[RdPage];
  uint32_t last = (RdPage + 1 < (int)RdPages.size()) ? RdPages[RdPage + 1] : RdLines.size();
  for (uint32_t li = first; li < last; li++) {
    RdLine &L = RdLines[li];
    if (L.flags & RL_HR) { tft.drawFastHLine(RD_W / 3, L.y, RD_W / 3, RdInk[8]); continue; }
    if (L.flags & RL_IMAGE) {
      RdItem &it = RdItems[L.first];
      size_t n;
      uint8_t *data = rdzipread(rdzipfind(String(RdPool + it.text)), &n);
      if (data) {
        int maxw = RD_W - L.x, maxh = L.imgh;
        if (data[0] == 0x89) tft.drawPng(data, n, L.x, L.y, maxw, maxh, 0, 0, L.gap, L.gap);
        else tft.drawJpg(data, n, L.x, L.y, maxw, maxh, 0, 0, L.gap, L.gap);
        free(data);
      }
      continue;
    }
    if (L.flags & RL_CODE) {
      int lh = L.size * 13 / 10;
      tft.fillRect(RD_MX + 2, L.y, 4, lh, RdInk[5]);
    }
    int asc = rdascent(0, L.size);
    float x = L.x;
    for (uint32_t k = L.first; k <= L.last; k++) {
      RdItem &w = RdItems[k];
      if (w.kind != RK_WORD) continue;
      if (k > L.first && w.space) x += L.gap;
      int ws = rdblocksize(w.block);
      rddrawtext(RdPool + w.text, w.len, (int)(x + 0.5f), L.y + asc, w.style, ws);
      x += w.w;
    }
  }
  char foot[64];
  snprintf(foot, sizeof(foot), "%d / %d", RdPage + 1, (int)RdPages.size());
  rdui(foot, RD_W - RD_MX, RD_H - 22, RD_UISIZE, 2, 0, 9);
  String chap = "Chapter " + String(RdChapter + 1) + " of " + String(RdSpine.size());
  for (auto &t : RdToc) if (t.chapter == RdChapter) { chap = t.title; break; }
  rdui(rdellipsize(chap, RD_W - 2 * RD_MX - 110, RD_UISIZE), RD_MX, RD_H - 22, RD_UISIZE, 0, 0, 9);
  tft.endWrite();
}

// ---- Navigation and position -----------------------------------------------------------

String rdpospath () { return RdBookPath + ".ulisp"; }

void rdsavepos () {
  if (!RdOpen || RdChapter < 0) return;
  uint32_t item = RdPages.empty() || RdLines.empty() ? 0 : RdLines[RdPages[RdPage]].first;
  String sub;
  fs::FS *fs = resolvepath(rdpospath().c_str(), sub);
  if (!fs) return;
  File f = fs->open(sub, "w", true);
  if (f) { f.printf("%d %u %d %s\n", RdChapter, item, RdFontSize, RdFamily.c_str()); f.close(); }
}

bool rdloadchapter (int ch) {
  if (ch < 0 || ch >= (int)RdSpine.size()) return false;
  size_t n;
  char *x = (char *)rdzipread(rdzipfind(RdSpine[ch]), &n);
  if (!x) { rdclearchapter(); RdChapter = ch; RdLines.clear(); RdPages.assign(1, 0); return true; }
  rdparsechapter(x, n, rddirof(RdSpine[ch]));
  free(x);
  RdChapter = ch;
  rdlayout();
  return true;
}

int rdpageofitem (uint32_t item) {
  for (int p = RdPages.size() - 1; p >= 0; p--) if (RdPages[p] < RdLines.size() && RdLines[RdPages[p]].first <= item) return p;
  return 0;
}

void rdclose () {
  if (RdOpen) rdsavepos();
  RdOpen = false;
  rdclearchapter();
  RdLines.clear(); RdPages.clear(); RdSpine.clear(); RdToc.clear();
  RdChapter = -1;
  if (RdZipFile) RdZipFile.close();
  RdZip.clear();
  free(RdPool); RdPool = NULL; RdPoolCap = RdPoolLen = 0;
}

bool rdhasbook () { return RdOpen && RdChapter >= 0; }

void rdcheckbook () { if (!rdhasbook()) error2("no book open; use book-open"); }

void rdcheckfonts () {
  if (!rdensurefonts()) error2("no fonts found: put .ttf or .otf files in /sd/fonts or /fonts");
  rdinitink();
}

// ---- Lisp functions ------------------------------------------------------------------------

object *fn_bookopen (object *args, object *env) {
  (void) env;
  String sub;
  fs::FS *fs = lisppath(first(args), sub);
  char buf[200]; cstring(first(args), buf, sizeof(buf));
  rdcheckfonts();
  rdclose();
  RdBookPath = buf; if (!RdBookPath.startsWith("/")) RdBookPath = "/" + RdBookPath;
  if (!rdzipopen(fs, sub)) error("not an EPUB (zip) file", first(args));
  if (!rdloadstructure()) { rdclose(); error("can't read the EPUB structure", first(args)); }
  RdOpen = true;
  int ch = 0; uint32_t item = 0;
  String psub;
  fs::FS *pfs = resolvepath(rdpospath().c_str(), psub);
  if (pfs && pfs->exists(psub)) {
    File f = pfs->open(psub, "r");
    if (f) {
      String line = f.readStringUntil('\n'); f.close();
      int c = 0, sz = 0; unsigned it = 0; char fam[64] = "";
      int got = sscanf(line.c_str(), "%d %u %d %63s", &c, &it, &sz, fam);
      if (got >= 2 && c >= 0 && c < (int)RdSpine.size()) { ch = c; item = it; }
      if (got >= 3 && sz >= 12 && sz <= 60) RdFontSize = sz;
      if (got >= 4 && String(fam) != RdFamily) rdloadfamily(fam);
    }
  }
  tft.setRotation(0);
  rdloadchapter(ch);
  RdPage = rdpageofitem(item);
  RdTurns = -1;  // first render uses quality mode
  rdrender();
  return rawstring(RdTitle.c_str(), RdTitle.length());
}

object *rdposition () {
  return cons(number(RdChapter), cons(number(RdPage), cons(number(RdPages.size()), NULL)));
}

object *fn_booknext (object *args, object *env) {
  (void) args, (void) env;
  rdcheckbook();
  if (RdPage + 1 < (int)RdPages.size()) RdPage++;
  else {
    int ch = RdChapter + 1;
    while (ch < (int)RdSpine.size()) { rdloadchapter(ch); if (!RdLines.empty()) break; ch++; }
    if (ch >= (int)RdSpine.size()) return nil;
    RdPage = 0;
  }
  rdrender(); rdsavepos();
  return rdposition();
}

object *fn_bookprev (object *args, object *env) {
  (void) args, (void) env;
  rdcheckbook();
  if (RdPage > 0) RdPage--;
  else {
    int ch = RdChapter - 1;
    while (ch >= 0) { rdloadchapter(ch); if (!RdLines.empty()) break; ch--; }
    if (ch < 0) { rdloadchapter(0); RdPage = 0; rdrender(); return nil; }
    RdPage = RdPages.size() - 1;
  }
  rdrender(); rdsavepos();
  return rdposition();
}

// (book-goto chapter [page])
object *fn_bookgoto (object *args, object *env) {
  (void) env;
  rdcheckbook();
  int ch = checkinteger(first(args));
  if (ch < 0 || ch >= (int)RdSpine.size()) error("no such chapter", first(args));
  rdloadchapter(ch);
  RdPage = 0;
  if (cdr(args) != NULL) { int p = checkinteger(second(args)); RdPage = p < 0 ? 0 : p >= (int)RdPages.size() ? RdPages.size() - 1 : p; }
  rdrender(); rdsavepos();
  return rdposition();
}

object *fn_booktoc (object *args, object *env) {
  (void) args, (void) env;
  rdcheckbook();
  object *head = cons(nil, NULL), *tail = head;
  if (RdToc.empty())
    for (size_t i = 0; i < RdSpine.size(); i++) { String t = "Chapter " + String(i + 1); cdr(tail) = cons(cons(lispstring((char *)t.c_str()), number(i)), NULL); tail = cdr(tail); }
  for (auto &t : RdToc) {
    String title = String(t.depth ? "  " : "") + t.title;
    cdr(tail) = cons(cons(rawstring(title.c_str(), title.length()), number(t.chapter)), NULL);
    tail = cdr(tail);
  }
  return cdr(head);
}

object *rdpair (const char *k, object *v) { return cons(lispstring((char *)k), v); }

object *fn_bookinfo (object *args, object *env) {
  (void) args, (void) env;
  if (!rdhasbook()) return nil;
  return cons(rdpair("title", rawstring(RdTitle.c_str(), RdTitle.length())),
         cons(rdpair("author", rawstring(RdAuthor.c_str(), RdAuthor.length())),
         cons(rdpair("path", rawstring(RdBookPath.c_str(), RdBookPath.length())),
         cons(rdpair("chapter", number(RdChapter)),
         cons(rdpair("chapters", number(RdSpine.size())),
         cons(rdpair("page", number(RdPage)),
         cons(rdpair("pages", number(RdPages.size())),
         cons(rdpair("font", rawstring(RdFamily.c_str(), RdFamily.length())),
         cons(rdpair("font-size", number(RdFontSize)), NULL)))))))));
}

// Relayouts the current chapter, keeping the reading position
void rdrelayout () {
  uint32_t item = RdPages.empty() || RdLines.empty() ? 0 : RdLines[RdPages[RdPage]].first;
  rdclearglyphs();
  rdlayout();
  RdPage = rdpageofitem(item);
  RdTurns = -1;
  rdrender(); rdsavepos();
}

object *fn_bookfontsize (object *args, object *env) {
  (void) env;
  if (args != NULL) {
    int s = checkinteger(first(args));
    if (s < 12 || s > 60) error("font size must be 12-60", first(args));
    RdFontSize = s;
    if (rdhasbook()) rdrelayout();
  }
  return number(RdFontSize);
}

object *fn_bookfont (object *args, object *env) {
  (void) env;
  if (args != NULL) {
    char fam[64]; if (!stringp(first(args))) error(notastring, first(args));
    cstring(first(args), fam, sizeof(fam));
    if (!RdFT && FT_Init_FreeType(&RdFT)) error2("FreeType failed to start");
    if (!rdloadfamily(fam)) error("font family not found in /sd/fonts or /fonts", first(args));
    if (rdhasbook()) rdrelayout();
  }
  rdensurefonts();
  return rawstring(RdFamily.c_str(), RdFamily.length());
}

object *fn_bookfonts (object *args, object *env) {
  (void) args, (void) env;
  std::vector<String> files, fams;
  rdfontfiles(files);
  for (auto &p : files) {
    String b = rdbasename(p); int r = b.indexOf("-Regular");
    String fam = r > 0 ? b.substring(0, r) : String("");
    if (!fam.length()) continue;
    bool seen = false; for (auto &f : fams) if (f == fam) seen = true;
    if (!seen) fams.push_back(fam);
  }
  object *head = cons(nil, NULL), *tail = head;
  for (auto &f : fams) { cdr(tail) = cons(rawstring(f.c_str(), f.length()), NULL); tail = cdr(tail); }
  return cdr(head);
}

object *fn_bookclose (object *args, object *env) {
  (void) args, (void) env;
  rdclose();
  tft.setEpdMode(epd_mode_t::epd_quality);
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  return nil;
}

object *fn_bookrender (object *args, object *env) {
  (void) args, (void) env;
  rdcheckbook();
  RdTurns = -1;
  rdrender();
  return rdposition();
}

// (draw-ttf string x y size [align] [style] [gray]) draws text in the reader font; y is the baseline
object *fn_drawttf (object *args, object *env) {
  (void) env;
  rdcheckfonts();
  object *str = first(args);
  if (!stringp(str)) error(notastring, str);
  lisptostracc(str);
  String s = StrAcc; StrAcc = "";
  int x = checkinteger(second(args)), y = checkinteger(third(args));
  args = cdr(cddr(args));
  int size = checkinteger(first(args)); args = cdr(args);
  int align = 0, style = 0, ink = 15;
  if (args) { align = checkinteger(first(args)); args = cdr(args); }
  if (args) { style = checkinteger(first(args)); args = cdr(args); }
  if (args) { ink = checkinteger(first(args)); if (ink < 0) ink = 0; if (ink > 15) ink = 15; ink = 15 - ink; args = cdr(args); }
  if (args && integerp(first(args))) s = rdellipsize(s, checkinteger(first(args)), size, style);
  if (size < 6 || size > 200) error2("size must be 6-200");
  tft.startWrite();
  int w = rdtextwidth(s.c_str(), s.length(), style, size);
  rdui(s, x, y, size, align, style, ink);
  tft.endWrite();
  return number(w);
}

object *fn_ttfwidth (object *args, object *env) {
  (void) env;
  rdcheckfonts();
  lisptostracc(first(args));
  String s = StrAcc; StrAcc = "";
  int size = checkinteger(second(args));
  int style = cddr(args) ? checkinteger(third(args)) : 0;
  return number(rdtextwidth(s.c_str(), s.length(), style, size));
}

const char doc_bookopen[] = "(book-open path)\n"
"Opens an EPUB, switches the screen to portrait and shows the page you last read. Returns the title.";
const char doc_booknext[] = "(book-next)\n"
"Shows the next page. Returns (chapter page pages), or nil at the end of the book.";
const char doc_bookprev[] = "(book-prev)\n"
"Shows the previous page. Returns (chapter page pages), or nil at the start.";
const char doc_bookgoto[] = "(book-goto chapter [page])\n"
"Jumps to a chapter (0-based, as returned by book-toc) and optionally a page within it.";
const char doc_booktoc[] = "(book-toc)\n"
"Returns the table of contents as a list of (title . chapter).";
const char doc_bookinfo[] = "(book-info)\n"
"Returns details of the open book as an association list, or nil if none is open.";
const char doc_bookfontsize[] = "(book-font-size [pixels])\n"
"Sets the body text size (12-60, default 26) and re-lays out the book. Returns the size.";
const char doc_bookfont[] = "(book-font [family])\n"
"Sets the font family, e.g. \"Caladea\", from /sd/fonts or /fonts. Returns the current family.";
const char doc_bookfonts[] = "(book-fonts)\n"
"Lists the font families available to the reader.";
const char doc_bookclose[] = "(book-close)\n"
"Saves the position, closes the book and returns the screen to landscape.";
const char doc_bookrender[] = "(book-render)\n"
"Redraws the current page with a full-quality refresh.";
const char doc_drawttf[] = "(draw-ttf string x y size [align] [style] [gray] [max-width])\n"
"Draws text in the reader font with its baseline at y. align: 0 left, 1 centre, 2 right;\n"
"style: 0 regular, 1 bold, 2 italic, 3 bold italic; gray 0 (black) to 15 (white).\n"
"Text wider than max-width is shortened with an ellipsis. Returns the width.";
const char doc_ttfwidth[] = "(ttf-width string size [style])\n"
"Returns the width in pixels of string in the reader font.";
