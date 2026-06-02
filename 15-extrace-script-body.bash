_rec_extract_script_body() {
    python3 - "$@" <<'PY'
import sys
import re
import shutil

if len(sys.argv) != 5:
    print("usage: _rec_extract_script_body src dst src.time dst.time", file=sys.stderr)
    sys.exit(1)

src = sys.argv[1]
dst = sys.argv[2]
time_src = sys.argv[3]
time_dst = sys.argv[4]

with open(src, "rb") as f:
    data = f.read()

total = 0
line_re = re.compile(r'^\s*([0-9.]+)\s+([0-9]+)\s*$')

with open(time_src, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        m = line_re.match(line)
        if m:
            total += int(m.group(2))

m = re.match(br'(?:\r?\n)?Script started on[^\r\n]*(?:\r\n|\n|\r)', data)
start = m.end() if m else 0

body = data[start:start + total]

if len(body) != total:
    print(f"rec: warning: body/timing mismatch: body={len(body)} timing={total} raw={len(data)}", file=sys.stderr)

with open(dst, "wb") as f:
    f.write(body)

shutil.copyfile(time_src, time_dst)
PY
}

_rec_drop_script_footer() {
    python3 - "$1" "$2" <<'PY'
import sys
import re

src = sys.argv[1]
dst = sys.argv[2]

with open(src, "rb") as f:
    data = f.read()

data = re.sub(br'(?:\r\n|\n|\r)?Script done on[^\r\n]*(?:\r\n|\n|\r)?\Z', b'', data, count=1)

with open(dst, "wb") as f:
    f.write(data)
PY
}

_rec_write_script_header() {
    printf 'Script started on %s\n' "$(date)"
}

_rec_write_script_footer() {
    printf '\nScript done on %s\n' "$(date)"
}

_rec_timing_size() {
    awk '{s += $2} END {print s + 0}' "$1"
}

_rec_replay_body_size() {
    python3 - "$1" <<'PY'
import sys
import re

with open(sys.argv[1], "rb") as f:
    data = f.read()

m = re.match(br'(?:\r?\n)?Script started on[^\r\n]*(?:\r\n|\n|\r)', data)
if m:
    data = data[m.end():]

data = re.sub(br'(?:\r\n|\n|\r)?Script done on[^\r\n]*(?:\r\n|\n|\r)?\Z', b'', data, count=1)

print(len(data))
PY
}