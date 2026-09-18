#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT="$PROJECT_ROOT/AIity.uproject"
REPORT_PARENT="${AIITY_TEST_REPORT_PARENT:-$PROJECT_ROOT/TestReports/AIity}"
TIMEOUT_SECONDS="${AIITY_TEST_TIMEOUT_SECONDS:-900}"

if [[ -n "${UE_ROOT:-}" ]]; then
	ENGINE_ROOT="$UE_ROOT"
else
	for candidate in \
		"/Users/Shared/Epic Games/UE_5.8"; do
		if [[ -x "$candidate/Engine/Binaries/Mac/UnrealEditor-Cmd" ]]; then
			ENGINE_ROOT="$candidate"
			break
		fi
	done
fi

EDITOR="${ENGINE_ROOT:-}/Engine/Binaries/Mac/UnrealEditor-Cmd"
if [[ -z "${ENGINE_ROOT:-}" || ! -x "$EDITOR" ]]; then
	echo "ERROR: UnrealEditor-Cmd not found. Set UE_ROOT to the verified Unreal 5.8 installation." >&2
	exit 2
fi

mkdir -p "$REPORT_PARENT"
if [[ ! -d "$REPORT_PARENT" ]]; then
	echo "ERROR: Automation report parent is not a directory: $REPORT_PARENT" >&2
	exit 3
fi
REPORT_DIR="$(mktemp -d "$REPORT_PARENT/run.XXXXXX")"
export EDITOR PROJECT REPORT_DIR TIMEOUT_SECONDS

python3 - <<'PY'
import json
import os
import pathlib
import stat
import subprocess
import sys

editor = os.environ["EDITOR"]
project = os.environ["PROJECT"]
report = pathlib.Path(os.environ["REPORT_DIR"])
timeout = int(os.environ["TIMEOUT_SECONDS"])
log_path = report / "engine-output.log"
command = [
    editor,
    project,
    "-unattended",
    "-NullRHI",
    "-nosplash",
    "-nosound",
    "-ExecCmds=Automation RunTests AIity.",
    "-TestExit=Automation Test Queue Empty",
    f"-ReportExportPath={report}",
    "-log",
]

def print_log_tail():
    print(f"Engine output log: {log_path}")
    try:
        with log_path.open("rb") as stream:
            stream.seek(0, os.SEEK_END)
            stream.seek(max(0, stream.tell() - 65536))
            lines = stream.read(65536).decode(errors="replace").splitlines()[-200:]
    except OSError as error:
        print(f"ERROR: Could not read engine output log: {error}", file=sys.stderr)
        return
    if lines:
        print("--- engine output tail (at most 200 lines / 64 KiB) ---")
        print("\n".join(lines))

try:
    with log_path.open("wb") as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                                timeout=timeout)
except subprocess.TimeoutExpired:
    print_log_tail()
    print(f"ERROR: AIity Automation tests timed out after {timeout}s.", file=sys.stderr)
    raise SystemExit(124)

print_log_tail()
if result.returncode:
    print(f"ERROR: Unreal test process exited {result.returncode}.", file=sys.stderr)
    raise SystemExit(result.returncode)

reports = []
for path in report.rglob("*.json"):
    try:
        if stat.S_ISREG(path.lstat().st_mode):
            reports.append(path)
    except OSError:
        continue
if not reports:
    print("ERROR: Unreal produced no JSON Automation report.", file=sys.stderr)
    raise SystemExit(3)

total = 0
failed = 0
incomplete = False
for path in reports:
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError):
        continue
    if isinstance(data, dict) and all(key in data for key in ("succeeded", "failed", "notRun")):
        if "inProcess" not in data:
            incomplete = True
            continue
        in_process = int(data["inProcess"])
        total = max(total, int(data["succeeded"]) + int(data.get("succeededWithWarnings", 0))
                    + int(data["failed"]) + int(data["notRun"]) + in_process)
        failed = max(failed, int(data["failed"]) + int(data["notRun"]) + in_process)

if incomplete:
    print("ERROR: Automation report is incomplete: native inProcess field is required.",
          file=sys.stderr)
    raise SystemExit(5)
if total == 0:
    print("ERROR: Automation report discovered zero AIity tests.", file=sys.stderr)
    raise SystemExit(4)
if failed:
    print(f"ERROR: Automation report contains {failed} failed or unrun tests.", file=sys.stderr)
    raise SystemExit(5)
print(f"AIity Automation gate passed: {total} tests.")
PY
