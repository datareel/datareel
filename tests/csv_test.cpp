// csv_test.cpp - 11/17/2026 (7.4): gxCSVReader and gxCSVWriter (gxcsv.h): RFC 4180 quoting, embedded delimiters,
// quotes and line breaks, CR LF and LF, the byte order mark, strict and lenient reading, trimming, other delimiters,
// the header, and random rows written and read back, in memory and through a file larger than the reader's piece
// (rows across piece boundaries). Usage: csv_test DIR. Prints PASS/FAIL lines; exit 0 when all pass.
#include <stdio.h>
#include <string>
#include <vector>
#include <random>
#include "gxcsv.h"
using namespace datareel;

typedef std::vector<std::string> Row;
typedef std::vector<Row> Rows;
static int pass = 0, fail = 0;
static void check(bool ok, const std::string &what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); (ok ? pass : fail)++; }
static std::string show(const Rows &r)
{
  std::string s;
  for(auto &row : r) { s += "["; for(size_t i = 0; i < row.size(); i++) { if(i) s += "|"; s += row[i]; } s += "]"; }
  for(auto &c : s) if(c == '\n') c = '~'; else if(c == '\r') c = '^';
  return s;
}
static Rows parse(const std::string &t, const gxCSVOptions &o = gxCSVOptions(), bool *ok = 0, std::string *why = 0)
{
  Rows r; std::string w; bool k = gxCSVReader::ParseText(t, r, o, &w);
  if(ok) *ok = k;
  if(why) *why = w;
  return r;
}
static void same(const Rows &got, const Rows &want, const std::string &what)
{
  check(got == want, what + (got == want ? "" : " [got " + show(got) + " want " + show(want) + "]"));
}

int main(int argc, char **argv)
{
  std::string dir = argc > 1 ? argv[1] : ".";
  // RFC 4180 basics
  same(parse("a,b,c\r\n1,2,3\r\n"), {{"a", "b", "c"}, {"1", "2", "3"}}, "rows with CR LF");
  same(parse("a,b\n1,2"), {{"a", "b"}, {"1", "2"}}, "LF and no line end on the last row");
  same(parse("a,,c,\n"), {{"a", "", "c", ""}}, "empty fields, a trailing empty field");
  same(parse("\"a,b\",\"say \"\"hi\"\"\",c\n"), {{"a,b", "say \"hi\"", "c"}}, "a quoted delimiter and doubled quotes");
  same(parse("\"line1\r\nline2\",x\n\"a\nb\",y\n"), {{"line1\r\nline2", "x"}, {"a\nb", "y"}}, "line breaks inside quotes kept as they are");
  same(parse("\"\",\"\"\n"), {{"", ""}}, "quoted empty fields");
  same(parse("a\n\nb\n"), {{"a"}, {""}, {"b"}}, "an empty line is a row with one empty field");
  { gxCSVOptions o; o.skip_empty_rows = true; same(parse("a\n\nb\n\n", o), {{"a"}, {"b"}}, "skip_empty_rows"); }
  { gxCSVOptions o; o.skip_empty_rows = true; same(parse("a\n\"\"\nb\n", o), {{"a"}, {""}, {"b"}}, "skip_empty_rows keeps a row of one quoted empty field"); }
  same(parse("\xEF\xBB\xBFname,qty\nx,1\n"), {{"name", "qty"}, {"x", "1"}}, "a UTF-8 byte order mark skipped");
  same(parse("caf\xC3\xA9,\xE2\x82\xAC" "5\n"), {{"caf\xC3\xA9", "\xE2\x82\xAC" "5"}}, "UTF-8 fields unchanged");
  // other delimiters, trimming
  { gxCSVOptions o; o.delimiter = ';'; same(parse("a;b,c;\"d;e\"\n", o), {{"a", "b,c", "d;e"}}, "delimiter ';'"); }
  { gxCSVOptions o; o.delimiter = '\t'; same(parse("a\tb c\t\"x\ty\"\n", o), {{"a", "b c", "x\ty"}}, "delimiter TAB"); }
  same(parse(" a , b \n"), {{" a ", " b "}}, "spaces kept by default (RFC 4180)");
  { gxCSVOptions o; o.trim_unquoted = true; same(parse(" a , \"  b \" , c\t\n", o), {{"a", "  b ", "c"}}, "trim_unquoted: spaces around unquoted fields and around quotes dropped, kept inside quotes"); }
  // strict and lenient
  { bool ok; std::string why; gxCSVOptions o; o.strict = true;
    parse("a,b\nc,d\"e\n", o, &ok, &why);
    check(!ok && why.find("line 2") != std::string::npos && why.find("field 2") != std::string::npos, "strict: a quote inside an unquoted field (" + why + ")");
    parse("a\n\"x\"y,z\n", o, &ok, &why);
    check(!ok && why.find("after a closing quote") != std::string::npos && why.find("line 2") != std::string::npos, "strict: text after a closing quote (" + why + ")");
    parse("a\n\"open\nmore\n", o, &ok, &why);
    check(!ok && why.find("never closed") != std::string::npos && why.find("line 2") != std::string::npos, "strict: a quoted field never closed (" + why + ")"); }
  same(parse("a,b\"c,\"x\"y\n"), {{"a", "b\"c", "xy"}}, "lenient: stray quotes read as text");
  same(parse("\"open,x\nmore"), {{"open,x\nmore"}}, "lenient: an unclosed quote runs to the end");
  // the header, Line(), Rows()
  { gxCSVReader r; r.SetText("id,name,note\n1,a,\"two\nlines\"\n2,b,c\n");
    bool h = r.ReadHeader(); Row row; r.ReadRow(row); unsigned long l1 = r.Line(); r.ReadRow(row); unsigned long l2 = r.Line();
    check(h && r.Column("name") == 1 && r.Column("nope") == -1 && l1 == 2 && l2 == 4 && r.Rows() == 3 && row[1] == "b",
	  "header, Column(), Line() counts lines inside quotes (" + std::to_string(l1) + ", " + std::to_string(l2) + ")"); }
  // the writer
  { gxCSVOptions o;
    check(gxCSVWriter::FormatRow({"a", "b,c", "say \"hi\"", "x\ny", " lead", "trail ", ""}, o) == "a,\"b,c\",\"say \"\"hi\"\"\",\"x\ny\",\" lead\",\"trail \",\r\n",
	  "writer: quotes only the fields that need it, CR LF");
    o.crlf = false; o.quote_all = true;
    check(gxCSVWriter::FormatRow({"a", "1"}, o) == "\"a\",\"1\"\n", "writer: quote_all, LF");
    o.quote_all = false;
    check(gxCSVWriter::FormatRow({""}, o) == "\"\"\n", "writer: a row of one empty field is written as \"\" (not an empty line)"); }
  // random rows: written and read back equal (memory, each option set)
  std::mt19937 rng(74);
  auto rnd_field = [&]() {
    static const char *bits[] = { "a", "Z", "0", ",", ";", "\t", "\"", "\"\"", " ", "\r\n", "\n", "\r", "x y", "\xC3\xA9", "" };
    std::string f; int n = (int)(rng() % 6);
    for(int i = 0; i < n; i++) f += bits[rng() % (sizeof(bits) / sizeof(bits[0]))];
    return f;
  };
  auto rnd_rows = [&](int n) { Rows r; for(int i = 0; i < n; i++) { Row row; int k = 1 + (int)(rng() % 7); for(int j = 0; j < k; j++) row.push_back(rnd_field()); r.push_back(row); } return r; };
  int bad = 0;
  for(int t = 0; t < 300; t++) {
    gxCSVOptions o; o.delimiter = ",;\t|"[rng() % 4]; o.crlf = rng() % 2; o.quote_all = rng() % 5 == 0; o.strict = true;
    Rows in = rnd_rows(1 + (int)(rng() % 20));
    std::string text; gxCSVWriter w(o); w.SetOutput(&text); for(auto &r : in) w.WriteRow(r);
    bool ok; Rows back = parse(text, o, &ok);
    if(!ok || back != in) { if(!bad) printf("  first difference: %s\n  vs %s\n", show(in).c_str(), show(back).c_str()); bad++; }
  }
  check(bad == 0, "300 random tables written and read back equal, strict (" + std::to_string(bad) + " differ)");
  // through a file larger than the reader's 64 KB piece: rows and quoted line breaks across piece boundaries
  {
    std::string fn = dir + "/csv_test_big.csv";
    Rows in = rnd_rows(40000);
    gxCSVOptions o; o.strict = true;
    gxCSVWriter w(o); bool wo = w.Open(fn); for(auto &r : in) w.WriteRow(r); bool wc = w.Close();
    gxCSVReader r(o); bool ro = r.Open(fn); Rows back; Row row; while(r.ReadRow(row)) back.push_back(row);
    FILE *f = fopen(fn.c_str(), "rb"); long size = 0; if(f) { fseek(f, 0, SEEK_END); size = ftell(f); fclose(f); }
    remove(fn.c_str());
    check(wo && wc && ro && !r.Error() && back == in && size > 4 * 65536,
	  "40000 random rows through a " + std::to_string(size / 1024) + " KB file read back equal (" + std::to_string(back.size()) + " rows" +
	  (r.Error() ? ", " + r.ErrorMessage() : "") + ")");
  }
  // errors: a file that is not there, a write to nowhere
  { gxCSVReader r; check(!r.Open(dir + "/no_such_file.csv") && r.Error(), "Open() of a missing file fails with Error()");
    gxCSVWriter w; check(!w.WriteRow({"a"}) && w.Error(), "WriteRow() with no output fails"); }
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
