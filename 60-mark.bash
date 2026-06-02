mark() {
    local msg="${*:-MARK}"
    printf '\033[1A\r\033[K'
    printf '\n===== %s =====\n\n' "$msg"
}