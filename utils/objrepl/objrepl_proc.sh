# objrepl_proc.sh - 11/17/2026 (7.4): sourced by the objrepl test scripts. Stopping a node the same way on every
# platform. Under Git Bash on Windows, kill -TERM ends a native program at once (TerminateProcess), with no clean
# close: a copy is stopped with ROOT/objrepl.stop instead (it stops within a second and removes the file), and a
# node's status file names its Windows process id, not the Git Bash one.
#
#   objrepl_pid PID          the process id the node writes in its status file (Windows: the job's native pid)
#   objrepl_stop PID [ROOT]  the clean stop: TERM (POSIX); ROOT/objrepl.stop (Windows; without ROOT, the node
#                            folder under $T whose status file names the process). Does not wait.
# It also keeps Git Bash from turning openssl's -subj "/CN=..." into a Windows path.
export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) OBJREPL_WIN=1 ;; *) OBJREPL_WIN=0 ;; esac
objrepl_pid() {
  if [ $OBJREPL_WIN = 1 ] && [ -r "/proc/$1/winpid" ]; then cat "/proc/$1/winpid"; else echo "$1"; fi
}
objrepl_stop() {
  local p=$1 root=${2:-} w f
  [ -n "$p" ] || return 0
  if [ $OBJREPL_WIN = 0 ]; then kill -TERM "$p" 2>/dev/null; return 0; fi
  if [ -z "$root" ]; then
    w=$(objrepl_pid "$p")
    for f in "${T:-.}"/*/objrepl.status "${T:-.}"/*/*/objrepl.status; do
      [ -f "$f" ] && [ "$(awk '$1 == "pid" { print $2 }' "$f")" == "$w" ] && { root=$(dirname "$f"); break; }
    done
  fi
  if [ -n "$root" ]; then touch "$root/objrepl.stop"; else kill -TERM "$p" 2>/dev/null; fi
}
