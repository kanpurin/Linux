_rec_current_terminal_size() {
    local size
    local rows
    local cols

    size=$(stty size 2>/dev/null) || return 1
    rows=${size%% *}
    cols=${size##* }

    case "$rows" in
        ''|*[!0-9]*)
            return 1
            ;;
    esac

    case "$cols" in
        ''|*[!0-9]*)
            return 1
            ;;
    esac

    printf '%s %s\n' "$rows" "$cols"
}

_rec_strip_timing_meta() {
    python3 - "$1" "$2" <<'PY'
import sys

src = sys.argv[1]
dst = sys.argv[2]

with open(src, "rb") as f:
    lines = f.readlines()

out = []
in_meta = False

for line in lines:
    s = line.strip()

    if s == b"# REC_META_BEGIN":
        in_meta = True
        continue

    if in_meta:
        if s == b"# REC_META_END":
            in_meta = False
        continue

    out.append(line)

with open(dst, "wb") as f:
    f.writelines(out)
PY
}

_rec_load_timing_meta() {
    python3 - "$1" <<'PY'
import sys

path = sys.argv[1]

try:
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
except OSError:
    sys.exit(1)

in_meta = False
meta = {}

for line in lines:
    s = line.strip()

    if s == "# REC_META_BEGIN":
        in_meta = True
        meta = {}
        continue

    if s == "# REC_META_END":
        if in_meta:
            break
        continue

    if not in_meta:
        continue

    if s.startswith("# "):
        s = s[2:]

    if "=" in s:
        k, v = s.split("=", 1)
        meta[k.strip()] = v.strip()

rows = meta.get("rec.rows")
cols = meta.get("rec.cols")
mixed = meta.get("rec.size_mixed", "0")

if not rows or not cols:
    sys.exit(1)

if not rows.isdigit() or not cols.isdigit():
    sys.exit(1)

if mixed != "1":
    mixed = "0"

print(rows, cols, mixed)
PY
}

_rec_write_timing_meta() {
    local time_file="$1"
    local rows="$2"
    local cols="$3"
    local mixed="${4:-0}"
    local tmp

    case "$rows" in
        ''|*[!0-9]*)
            return 1
            ;;
    esac

    case "$cols" in
        ''|*[!0-9]*)
            return 1
            ;;
    esac

    case "$mixed" in
        1)
            mixed=1
            ;;
        *)
            mixed=0
            ;;
    esac

    tmp=$(mktemp)
    _rec_strip_timing_meta "$time_file" "$tmp"
    cat "$tmp" > "$time_file"
    rm -f "$tmp"

    if [ -s "$time_file" ]; then
        if [ "$(tail -c 1 "$time_file" | od -An -t x1 | tr -d ' ')" != "0a" ]; then
            printf '\n' >> "$time_file"
        fi
    fi

    {
        printf '# REC_META_BEGIN\n'
        printf '# rec.rows=%s\n' "$rows"
        printf '# rec.cols=%s\n' "$cols"
        printf '# rec.size_mixed=%s\n' "$mixed"
        printf '# REC_META_END\n'
    } >> "$time_file"
}

_rec_check_terminal_size_for_replay() {
    local time_file="$1"
    local cur
    local meta
    local cur_rows
    local cur_cols
    local rec_rows
    local rec_cols
    local mixed
    local warned=0

    cur=$(_rec_current_terminal_size) || {
        echo "play: warning: current terminal size is unknown" >&2
        echo "play: warning: replay may be broken" >&2
        return 1
    }

    set -- $cur
    cur_rows="$1"
    cur_cols="$2"

    meta=$(_rec_load_timing_meta "$time_file")
    if [ $? -ne 0 ]; then
        echo "play: warning: terminal size metadata is missing" >&2
        echo "play: warning: current terminal size: rows=${cur_rows} cols=${cur_cols}" >&2
        echo "play: warning: replay may be broken if terminal size differs from recording" >&2
        return 1
    fi

    set -- $meta
    rec_rows="$1"
    rec_cols="$2"
    mixed="$3"

    if [ "$mixed" = "1" ]; then
        echo "play: warning: recording contains multiple or unknown terminal sizes" >&2
        echo "play: warning: recorded terminal size: rows=${rec_rows} cols=${rec_cols}" >&2
        echo "play: warning: current terminal size:  rows=${cur_rows} cols=${cur_cols}" >&2
        echo "play: warning: replay may be broken" >&2
        warned=1
    fi

    if [ "$rec_rows" != "$cur_rows" ] || [ "$rec_cols" != "$cur_cols" ]; then
        echo "play: warning: terminal size differs" >&2
        echo "play: warning: recorded terminal size: rows=${rec_rows} cols=${rec_cols}" >&2
        echo "play: warning: current terminal size:  rows=${cur_rows} cols=${cur_cols}" >&2
        echo "play: warning: replay may be broken" >&2
        warned=1
    fi

    return "$warned"
}