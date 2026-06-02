_rec_filter_novim() {
    python3 - "$@" <<'PY'
import sys
import re

if len(sys.argv) not in (3, 5):
    print("usage: _rec_filter_novim src dst [src.time dst.time]", file=sys.stderr)
    sys.exit(1)

src = sys.argv[1]
dst = sys.argv[2]
time_src = sys.argv[3] if len(sys.argv) == 5 else None
time_dst = sys.argv[4] if len(sys.argv) == 5 else None

with open(src, "rb") as f:
    data = f.read()

enter_re = re.compile(br'\x1b\[\?(?:1049|1047|1048|47)h')
exit_re = re.compile(br'\x1b\[\?(?:1049|1047|1048|47)l')

keep = []
pos = 0
n = len(data)

while pos < n:
    m = enter_re.search(data, pos)
    if not m:
        keep.append((pos, n))
        break

    if pos < m.start():
        keep.append((pos, m.start()))

    e = exit_re.search(data, m.end())
    if not e:
        break

    pos = e.end()

filtered = b"".join(data[a:b] for a, b in keep)

with open(dst, "wb") as f:
    f.write(filtered)

if time_src and time_dst:
    line_re = re.compile(r'^\s*([0-9.]+)\s+([0-9]+)\s*$')

    out_lines = []
    data_pos = 0
    keep_i = 0

    with open(time_src, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = line_re.match(line)
            if not m:
                continue

            delay = m.group(1)
            count = int(m.group(2))

            start = data_pos
            end = data_pos + count
            data_pos = end

            while keep_i < len(keep) and keep[keep_i][1] <= start:
                keep_i += 1

            j = keep_i
            first = True

            while j < len(keep) and keep[j][0] < end:
                a = max(start, keep[j][0])
                b = min(end, keep[j][1])

                if a < b:
                    kept_count = b - a
                    kept_delay = delay if first else "0.000000"
                    out_lines.append(f"{kept_delay} {kept_count}\n")
                    first = False

                j += 1

    with open(time_dst, "w", encoding="utf-8") as f:
        f.writelines(out_lines)
PY
}