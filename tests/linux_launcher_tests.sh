#!/bin/bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "usage: linux_launcher_tests.sh <ExeFileConsole> <stub> [accepted-exefile]" >&2
  exit 2
fi

LAUNCHER=$1
STUB=$2
ACCEPTED_EXEFILE=${3:-}
ROOT=$(mktemp -d)
export TEST_PROPAGATE=kept
STDOUT_FILE=$ROOT/stdout
STDERR_FILE=$ROOT/stderr

run_contract() {
  local dir=$1
  mkdir -p "$dir"
  cp "$LAUNCHER" "$dir/ExeFileConsole"
  cp "$STUB" "$dir/ExeFile"
  chmod +x "$dir/ExeFileConsole" "$dir/ExeFile"

  set +e
  printf 'ping' | "$dir/ExeFileConsole" alpha 'beta gamma' '$(echo pwned)' --exit=0 >"$STDOUT_FILE" 2>"$STDERR_FILE"
  local status=$?
  set -e
  if [ "$status" -ne 0 ]; then
    echo "expected stub success in $dir, got $status" >&2
    cat "$STDOUT_FILE" "$STDERR_FILE" >&2
    exit 1
  fi
  grep -F "PATH=$dir/ExeFile" "$STDOUT_FILE"
  grep -F "ARGV1=alpha" "$STDOUT_FILE"
  grep -F "ARGV2=beta gamma" "$STDOUT_FILE"
  grep -F 'ARGV3=$(echo pwned)' "$STDOUT_FILE"
  grep -F "INHERIT=" "$STDOUT_FILE"
  grep -F "PROP=kept" "$STDOUT_FILE"
  grep -F "STDIN=ping" "$STDOUT_FILE"
  grep -F "STUB_ERR" "$STDERR_FILE"
  if grep -F "/bin/sh" "$STDOUT_FILE"; then
    echo "child path was a shell" >&2
    exit 1
  fi

  set +e
  "$dir/ExeFileConsole" --exit=7 >"$STDOUT_FILE" 2>"$STDERR_FILE"
  status=$?
  set -e
  if [ "$status" -ne 7 ]; then
    echo "expected child exit 7 in $dir, got $status" >&2
    exit 1
  fi

  set +e
  "$dir/ExeFileConsole" --signal=15 >"$STDOUT_FILE" 2>"$STDERR_FILE"
  status=$?
  set -e
  if [ "$status" -ne 143 ]; then
    echo "expected signal exit 143 in $dir, got $status" >&2
    exit 1
  fi

  rm -f "$dir/ExeFile"
  set +e
  "$dir/ExeFileConsole" alpha >"$STDOUT_FILE" 2>"$STDERR_FILE"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    echo "missing executable returned success in $dir" >&2
    exit 1
  fi
  grep -F "exec failed" "$STDERR_FILE"
}

run_contract "$ROOT/dir with spaces"
run_contract "$ROOT/Console parent/dir with spaces"
DIR="$ROOT/dir with spaces"

if [ -n "$ACCEPTED_EXEFILE" ]; then
  cp "$ACCEPTED_EXEFILE" "$DIR/ExeFile"
  chmod +x "$DIR/ExeFile"
  DIRECT_OUT=$ROOT/direct.out
  DIRECT_ERR=$ROOT/direct.err
  CONSOLE_OUT=$ROOT/console.out
  CONSOLE_ERR=$ROOT/console.err
  set +e
  EXEFILE_INHERIT= "$DIR/ExeFile" /nocrashreportupload /verbose >"$DIRECT_OUT" 2>"$DIRECT_ERR"
  DIRECT_STATUS=$?
  "$DIR/ExeFileConsole" /nocrashreportupload /verbose >"$CONSOLE_OUT" 2>"$CONSOLE_ERR"
  CONSOLE_STATUS=$?
  set -e
  if [ "$DIRECT_STATUS" -ne "$CONSOLE_STATUS" ]; then
    echo "exefile exit $DIRECT_STATUS did not match console exit $CONSOLE_STATUS" >&2
    echo "direct stderr:" >&2
    cat "$DIRECT_ERR" >&2
    echo "console stderr:" >&2
    cat "$CONSOLE_ERR" >&2
    exit 1
  fi
  if ! grep -F "Failed to load" "$CONSOLE_ERR" >/dev/null && ! grep -F "starting up" "$CONSOLE_OUT" >/dev/null; then
    echo "accepted exefile did not run through the console" >&2
    cat "$CONSOLE_OUT" "$CONSOLE_ERR" >&2
    exit 1
  fi
  echo "EXEFILE_LAUNCH status=$CONSOLE_STATUS upload_flag=/nocrashreportupload"
fi

echo "LINUX_EXEFILECONSOLE_OK"
