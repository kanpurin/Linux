_rec_clean_control() {
    python3 - "$1" "$2" "${3:-80}" <<'PY'
import sys

src = sys.argv[1]
dst = sys.argv[2]

try:
    cols = int(sys.argv[3])
except Exception:
    cols = 80

if cols <= 0:
    cols = 80

with open(src, "rb") as f:
    data = f.read()

lines = [bytearray()]
soft = [False]
row = 0
col = 0
wrap_pending = False
i = 0

def ensure_row(r):
    while len(lines) <= r:
        lines.append(bytearray())
        soft.append(False)

def mark_soft(r, value):
    ensure_row(r)
    soft[r] = value

def clamp_col():
    global col
    if col < 0:
        col = 0
    if col >= cols:
        col = cols - 1

def clear_wrap_pending():
    global wrap_pending, col
    if wrap_pending:
        wrap_pending = False
        col = cols - 1

def before_printable():
    global row, col, wrap_pending
    if wrap_pending:
        row += 1
        col = 0
        wrap_pending = False
        ensure_row(row)
        mark_soft(row, True)

def hard_newline():
    global row, col, wrap_pending
    wrap_pending = False
    row += 1
    col = 0
    ensure_row(row)
    mark_soft(row, False)

def write_byte(b):
    global col, wrap_pending

    before_printable()

    line = lines[row]
    while len(line) < col:
        line.append(0x20)

    if col < len(line):
        line[col] = b
    else:
        line.append(b)

    if col >= cols - 1:
        col = cols - 1
        wrap_pending = True
    else:
        col += 1

def parse_params(s):
    s = s.replace("?", "")
    if s == "":
        return []
    out = []
    for x in s.split(";"):
        if x == "":
            out.append(None)
        else:
            try:
                out.append(int(x))
            except ValueError:
                out.append(None)
    return out

def p(params, idx, default):
    if idx >= len(params) or params[idx] is None:
        return default
    return params[idx]

while i < len(data):
    b = data[i]

    if b == 0x1b:
        if i + 1 < len(data) and data[i + 1] == ord("["):
            j = i + 2
            while j < len(data) and not (0x40 <= data[j] <= 0x7e):
                j += 1
            if j >= len(data):
                break

            body = data[i + 2:j].decode("ascii", errors="ignore")
            final = chr(data[j])
            params = parse_params(body)

            clear_wrap_pending()

            if final == "A":
                row = max(0, row - p(params, 0, 1))
                ensure_row(row)
                clamp_col()
            elif final == "B":
                row += p(params, 0, 1)
                ensure_row(row)
                clamp_col()
            elif final == "C":
                col += p(params, 0, 1)
                clamp_col()
            elif final == "D":
                col = max(0, col - p(params, 0, 1))
            elif final == "E":
                row += p(params, 0, 1)
                ensure_row(row)
                col = 0
                mark_soft(row, False)
            elif final == "F":
                row = max(0, row - p(params, 0, 1))
                ensure_row(row)
                col = 0
            elif final == "G":
                col = max(0, p(params, 0, 1) - 1)
                clamp_col()
            elif final in ("H", "f"):
                row = max(0, p(params, 0, 1) - 1)
                col = max(0, p(params, 1, 1) - 1)
                ensure_row(row)
                clamp_col()
            elif final == "@":
                n = max(1, p(params, 0, 1))
                line = lines[row]
                while len(line) < col:
                    line.append(0x20)
                line[col:col] = b" " * n
            elif final == "P":
                n = max(1, p(params, 0, 1))
                line = lines[row]
                if col < len(line):
                    del line[col:col + n]
            elif final == "X":
                n = max(1, p(params, 0, 1))
                line = lines[row]
                while len(line) < col + n:
                    line.append(0x20)
                for k in range(col, col + n):
                    line[k] = 0x20
            elif final == "K":
                mode = p(params, 0, 0)
                line = lines[row]
                if mode == 0:
                    del line[col:]
                elif mode == 1:
                    end = min(col + 1, len(line))
                    for k in range(end):
                        line[k] = 0x20
                elif mode == 2:
                    lines[row] = bytearray()
                    col = 0
            elif final == "J":
                mode = p(params, 0, 0)
                if mode == 2:
                    lines = [bytearray()]
                    soft = [False]
                    row = 0
                    col = 0
                    wrap_pending = False

            i = j + 1
            continue

        if i + 1 < len(data) and data[i + 1] == ord("]"):
            j = i + 2
            while j < len(data):
                if data[j] == 0x07:
                    j += 1
                    break
                if data[j] == 0x1b and j + 1 < len(data) and data[j + 1] == ord("\\"):
                    j += 2
                    break
                j += 1
            i = j
            continue

        i += 2
        continue

    if b == 0x07:
        i += 1
        continue

    if b in (0x08, 0x7f):
        clear_wrap_pending()
        col = max(0, col - 1)
        i += 1
        continue

    if b == 0x0d:
        wrap_pending = False
        col = 0
        if i + 1 < len(data) and data[i + 1] == 0x0a:
            hard_newline()
            i += 2
        else:
            i += 1
        continue

    if b == 0x0a:
        hard_newline()
        i += 1
        continue

    if b < 32 and b != 0x09:
        i += 1
        continue

    write_byte(b)
    i += 1

logical = []

for idx, line in enumerate(lines):
    text = bytes(line).rstrip()

    if idx > 0 and soft[idx]:
        if logical:
            logical[-1] += text
        else:
            logical.append(text)
    else:
        logical.append(text)

out = b"\n".join(logical)

if data.endswith(b"\n") or data.endswith(b"\r\n"):
    out += b"\n"

with open(dst, "wb") as f:
    f.write(out)
PY
}