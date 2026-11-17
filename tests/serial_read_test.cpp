// serial_read_test.cpp - 11/17/2026 (7.4): the serial_read example on a pseudo terminal (POSIX, no hardware): a pty's
// slave is the "serial port", this program writes to the master. Bytes of every value (binary mode), the --hex form,
// the --bytes and --seconds limits, a hangup (the master closed: exit 0) and SIGTERM. Usage: serial_read_test
// SERIAL_READ DIR. Prints PASS/FAIL lines; exit 0 when all pass.
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

static int pass = 0, fail = 0;
static void check(bool ok, const std::string &what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); fflush(stdout); (ok ? pass : fail)++; }

static std::string slurp(const std::string &fn)
{
  std::string s; FILE *f = fopen(fn.c_str(), "rb"); if(!f) return s;
  char b[4096]; size_t n; while((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n); fclose(f); return s;
}

// a pty pair: the master's fd, the slave's name
static bool make_pty(int &m, std::string &slave)
{
  m = posix_openpt(O_RDWR | O_NOCTTY);
  if(m < 0 || grantpt(m) != 0 || unlockpt(m) != 0) return false;
  fcntl(m, F_SETFD, FD_CLOEXEC);   // serial_read must not hold the master too, or closing it here is no hangup
  const char *n = ptsname(m); if(!n) return false;
  slave = n;
  struct termios t; if(tcgetattr(m, &t) == 0) { cfmakeraw(&t); tcsetattr(m, TCSANOW, &t); }
  return true;
}

// serial_read with ARGS in a child; its pid
static pid_t run(const std::string &prog, const std::vector<std::string> &args)
{
  pid_t k = fork();
  if(k == 0) {
    std::vector<char *> av; av.push_back((char *)prog.c_str());
    for(auto &a : args) av.push_back((char *)a.c_str());
    av.push_back(0);
    execv(prog.c_str(), av.data()); _exit(127);
  }
  return k;
}
static int wait_for(pid_t k, int secs)
{
  int st = 0;
  for(int i = 0; i < secs * 100; i++) { if(waitpid(k, &st, WNOHANG) == k) return WIFEXITED(st) ? WEXITSTATUS(st) : -WTERMSIG(st); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
  kill(k, SIGKILL); waitpid(k, &st, 0); return -999;
}

int main(int argc, char **argv)
{
  if(argc < 3) { fprintf(stderr, "usage: serial_read_test SERIAL_READ DIR\n"); return 64; }
  std::string prog = argv[1], dir = argv[2];
  std::string data; for(int r = 0; r < 4; r++) for(int i = 0; i < 256; i++) data += (char)i;   // every byte value, 1 KB
  // 1 binary: every byte value arrives as sent (--bytes ends the read)
  {
    int m; std::string sl; bool ok = make_pty(m, sl);
    std::string out = dir + "/sr_bin.out"; remove(out.c_str());
    pid_t k = ok ? run(prog, {sl, "--baud", "115200", "--out", out, "--bytes", std::to_string(data.size()), "--quiet"}) : -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    for(size_t i = 0; ok && i < data.size(); i += 100) { (void)!write(m, data.data() + i, std::min<size_t>(100, data.size() - i)); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    int rc = k > 0 ? wait_for(k, 10) : -1;
    std::string got = slurp(out);
    check(ok && rc == 0 && got == data, "binary: 1024 bytes of every value arrive unchanged, --bytes ends the read (rc " + std::to_string(rc) + ", " + std::to_string(got.size()) + " bytes)");
    if(m >= 0) close(m);
  }
  // 2 --hex: two hex digits a byte, 16 a line
  {
    int m; std::string sl; bool ok = make_pty(m, sl);
    std::string out = dir + "/sr_hex.out"; remove(out.c_str());
    pid_t k = ok ? run(prog, {sl, "--hex", "--out", out, "--bytes", "20", "--quiet"}) : -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::string d; for(int i = 0; i < 20; i++) d += (char)(0xF0 + (i % 16));
    if(ok) (void)!write(m, d.data(), d.size());
    int rc = k > 0 ? wait_for(k, 10) : -1;
    std::string got = slurp(out);
    std::string want = "F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA FB FC FD FE FF\nF0 F1 F2 F3\n";
    check(ok && rc == 0 && got == want, "--hex: 16 bytes a line in hex (rc " + std::to_string(rc) + ")");
    if(m >= 0) close(m);
  }
  // 3 --seconds with nothing arriving: ends on time, exit 0
  {
    int m; std::string sl; bool ok = make_pty(m, sl);
    auto t0 = std::chrono::steady_clock::now();
    pid_t k = ok ? run(prog, {sl, "--seconds", "1", "--out", dir + "/sr_idle.out", "--quiet"}) : -1;
    int rc = k > 0 ? wait_for(k, 10) : -1;
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(ok && rc == 0 && s >= 0.9 && s < 4, "--seconds 1 with no data: ends after " + std::to_string(s).substr(0, 4) + " s, rc " + std::to_string(rc));
    if(m >= 0) close(m);
  }
  // 4 a hangup: the master closed while reading: the data so far is kept, exit 0
  {
    int m; std::string sl; bool ok = make_pty(m, sl);
    std::string out = dir + "/sr_hup.out"; remove(out.c_str());
    pid_t k = ok ? run(prog, {sl, "--out", out, "--quiet"}) : -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if(ok) (void)!write(m, "before the hangup", 17);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if(m >= 0) close(m);
    int rc = k > 0 ? wait_for(k, 10) : -1;
    check(ok && rc == 0 && slurp(out) == "before the hangup", "a hangup (the pty master closed) ends the read with exit 0 and the data kept (rc " + std::to_string(rc) + ")");
  }
  // 5 SIGTERM stops it within a second, exit 0
  {
    int m; std::string sl; bool ok = make_pty(m, sl);
    pid_t k = ok ? run(prog, {sl, "--out", dir + "/sr_term.out", "--quiet"}) : -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    auto t0 = std::chrono::steady_clock::now();
    if(k > 0) kill(k, SIGTERM);
    int rc = k > 0 ? wait_for(k, 5) : -1;
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(ok && rc == 0 && s < 1.5, "SIGTERM: stopped in " + std::to_string(s).substr(0, 4) + " s, rc " + std::to_string(rc));
    if(m >= 0) close(m);
  }
  // 6 a device that is not there: exit 1 with a message
  { pid_t k = run(prog, {"/dev/no_such_tty", "--quiet"}); int rc = wait_for(k, 5); check(rc == 1, "a missing device: exit 1"); }
  printf("%d passed, %d failed\n", pass, fail);
  return fail ? 1 : 0;
}
