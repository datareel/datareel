// ansi_strip_test.cpp - 11/17/2026 (7.4): strip_ansi() and AnsiStripper (strutil.h): each kind of ECMA-48 control
// sequence, the options, UTF-8 text passed through, and text fed in pieces split at every byte (and at random
// sizes) giving the same result as one call. Prints PASS/FAIL lines and "N passed, M failed"; exit 0 when all pass.
#include <stdio.h>
#include <string>
#include <random>
#include "strutil.h"
using namespace datareel;

static int pass = 0, fail = 0;
static std::string vis(const std::string &s)
{
  std::string o;
  for(unsigned char c : s) {
    if(c == '\n') o += "\\n"; else if(c == '\r') o += "\\r"; else if(c == '\t') o += "\\t";
    else if(c == 0x1B) o += "\\e"; else if(c < 0x20 || c == 0x7F) { char b[8]; snprintf(b, sizeof(b), "\\x%02x", c); o += b; }
    else o += (char)c;
  }
  return o;
}
static void eq(const std::string &got, const std::string &want, const std::string &what)
{
  bool ok = got == want;
  printf("%s %s%s\n", ok ? "PASS" : "FAIL", what.c_str(), ok ? "" : (" [got \"" + vis(got) + "\" want \"" + vis(want) + "\"]").c_str());
  (ok ? pass : fail)++;
}
static std::string pieces(const std::string &in, const AnsiStripOptions &o, size_t cut)
{
  AnsiStripper a(o); std::string out;
  a.Feed(std::string_view(in).substr(0, cut), out); a.Feed(std::string_view(in).substr(cut), out); a.Finish(out);
  return out;
}

int main()
{
  AnsiStripOptions d;
  // CSI: colours, cursor moves, private modes, parameters and intermediates
  eq(strip_ansi("\x1b[1;31mred\x1b[0m plain"), "red plain", "CSI SGR colours");
  eq(strip_ansi("a\x1b[2Jb\x1b[10;20Hc\x1b[?25ld\x1b[?1049he"), "abcde", "CSI erase, cursor position, private modes");
  eq(strip_ansi("x\x1b[1 qy"), "xy", "CSI with an intermediate byte (DECSCUSR)");
  eq(strip_ansi("x\x1b[38;2;10;20;30mY"), "xY", "CSI true colour");
  // OSC ended by BEL and by ST
  eq(strip_ansi("\x1b]0;window title\x07text"), "text", "OSC title ended by BEL");
  eq(strip_ansi("\x1b]8;;http://x/\x1b\\link\x1b]8;;\x1b\\ after"), "link after", "OSC 8 hyperlink ended by ST");
  // DCS, SOS, PM, APC
  eq(strip_ansi("a\x1bPq#0;2;0;0;0\x1b\\b"), "ab", "DCS (sixel) to ST");
  eq(strip_ansi("a\x1b_apc data\x1b\\b\x1b^pm\x1b\\c\x1bXsos\x1b\\d"), "abcd", "APC, PM, SOS to ST");
  // ESC with one final character, with intermediates (charsets)
  eq(strip_ansi("a\x1b" "7b\x1b" "8c\x1b" "cd\x1b(Be\x1b)0f\x1b#8g"), "abcdefg", "ESC 7/8/c, charset designations, DECALN");
  // C0 controls
  eq(strip_ansi(std::string("a\x07" "b\x08" "c\x0e" "d\x0f" "e\0f", 11)), "abcdef", "C0 controls out (BEL, BS, SO, SI, NUL)");
  eq(strip_ansi("col1\tcol2\nline2"), "col1\tcol2\nline2", "tab and newline kept");
  { AnsiStripOptions o; o.keep_tab = false; o.keep_newline = false; eq(strip_ansi("a\tb\nc", o), "abc", "keep_tab and keep_newline off"); }
  eq(strip_ansi("a\rb"), "ab", "a lone CR dropped (default)");
  { AnsiStripOptions o; o.cr_to_newline = true;
    eq(strip_ansi("a\rb\r\nc\r", o), "a\nb\nc\n", "cr_to_newline: CR, CRLF and a CR at the end, one newline each");
    eq(strip_ansi("a\r\x1b[1mb", o), "a\nb", "cr_to_newline: CR, a sequence, text"); }
  eq(strip_ansi("a\x7f" "b"), "ab", "DEL dropped (default)");
  { AnsiStripOptions o; o.keep_del = true; eq(strip_ansi("a\x7f" "b", o), "a\x7f" "b", "keep_del"); }
  // cursor moves as line breaks (vt220-strip)
  { AnsiStripOptions o; o.cursor_to_newline = true;
    eq(strip_ansi("\x1b[1;1HTEMP 72\x1b[2;1HWIND 270/10\x1b[3;1fALT 30.01", o), "\nTEMP 72\nWIND 270/10\nALT 30.01",
       "cursor_to_newline: CSI H and f become line breaks");
    eq(strip_ansi("a\x1b[Bb\x1b" "Ec", o), "a\nb\nc", "cursor_to_newline: cursor down and NEL"); }
  // unfinished and broken sequences
  eq(strip_ansi("text\x1b[12"), "text", "an unfinished CSI at the end is dropped");
  eq(strip_ansi("a\x1b[12\x1b[1mb"), "ab", "a CSI cut short by a new ESC");
  // ECMA-48 / the DEC parser: the C0 is acted on and the CSI goes on, so the next byte in 0x40-0x7E ends it
  eq(strip_ansi("a\x1b[1\nmb"), "a\nb", "a C0 control inside a CSI is acted on, the CSI goes on to its final byte");
  eq(strip_ansi("a\x1b\x1b[1mb"), "ab", "ESC ESC [");
  eq(strip_ansi("\x1b]unterminated title"), "", "an OSC never ended swallows the rest");
  // UTF-8 passes through; 8-bit C1 only with c1_8bit
  std::string u8 = "caf\xc3\xa9 \xe2\x82\xac 10 \xf0\x9f\x93\x88";
  eq(strip_ansi(u8 + "\x1b[0m"), u8, "UTF-8 text (bytes 0x80-0x9F as continuation bytes) unchanged");
  { AnsiStripOptions o; o.c1_8bit = true;
    eq(strip_ansi("a\x9b" "1;31mb\x9d" "0;t\x07" "c\x90" "dcs\x9c" "d\x85" "e", o), "abcde", "c1_8bit: 8-bit CSI, OSC, DCS/ST, NEL"); }
  // counts
  { AnsiStripper a; std::string out; a.Feed("\x1b[1mA\x1b[0m\x07", out); a.Finish(out);
    eq(out + "/" + std::to_string(a.Removed()), "A/3", "Removed(): two sequences and one control"); }
  // pieces: every split point of a mixed text equals one call, for each option set
  std::string mixed = "\x1b[1;1H\x1b]0;t\x07hello\r\n\x1b[31mred\x1b[0m\t\x1bPq#0\x1b\\caf\xc3\xa9\x1b(B\x1b[10;5fend\r";
  AnsiStripOptions sets[4]; sets[1].cr_to_newline = true; sets[2].cursor_to_newline = true; sets[3].keep_tab = sets[3].keep_newline = false;
  for(int k = 0; k < 4; k++) {
    std::string one = strip_ansi(mixed, sets[k]);
    int bad = 0; size_t first = 0;
    for(size_t c = 0; c <= mixed.size(); c++) if(pieces(mixed, sets[k], c) != one) { if(!bad) first = c; bad++; }
    eq(bad ? "split at " + std::to_string(first) + " differs (" + std::to_string(bad) + ")" : "same", "same",
       "two pieces split at each of " + std::to_string(mixed.size() + 1) + " points equal one call (option set " + std::to_string(k) + ")");
  }
  // random texts in random pieces
  std::mt19937 r(2026);
  const char *parts[] = { "abc", " ", "\t", "\n", "\r", "\r\n", "\x1b[1;2m", "\x1b[H", "\x1b]2;x\x07", "\x1b]8;;u\x1b\\", "\x1bPz\x1b\\",
			  "\x1b" "7", "\x1b(0", "\x07", "\x08", "\xc3\xa9", "\x1b[?25h", "\x1b[5;7f", "XYZ" };
  int rbad = 0;
  for(int t = 0; t < 2000; t++) {
    std::string in; int n = 1 + (int)(r() % 30);
    for(int i = 0; i < n; i++) in += parts[r() % (sizeof(parts) / sizeof(parts[0]))];
    AnsiStripOptions o = sets[r() % 4];
    std::string one = strip_ansi(in, o);
    AnsiStripper a(o); std::string out;
    for(size_t i = 0; i < in.size();) { size_t m = 1 + r() % 7; a.Feed(std::string_view(in).substr(i, m), out); i += m; }
    a.Finish(out);
    bool clean = one.find('\x1b') == std::string::npos;
    if(out != one || !clean) rbad++;
  }
  eq(std::to_string(rbad), "0", "2000 random texts fed in random pieces equal one call, and no ESC is left");
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
