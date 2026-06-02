_rec_append_timing() {
    local src_time="$1"
    local dst_time="$2"

    if [ -s "$dst_time" ]; then
        if [ "$(tail -c 1 "$dst_time" | od -An -t x1 | tr -d ' ')" != "0a" ]; then
            printf '\n' >> "$dst_time"
        fi
    fi

    cat "$src_time" >> "$dst_time"
}

_rec_validate_timing() {
    local log_file="$1"
    local time_file="$2"
    local body_size
    local time_size

    [ -f "$log_file" ] || return 1
    [ -f "$time_file" ] || return 1

    body_size=$(_rec_replay_body_size "$log_file")
    time_size=$(_rec_timing_size "$time_file")

    if [ "$body_size" != "$time_size" ]; then
        echo "rec: warning: timing/body size mismatch: body=$body_size timing=$time_size" >&2
        return 1
    fi

    return 0
}

rec() {
    local with_time=0
    local novim=0
    local file=""
    local write_mode=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --time)
                with_time=1
                shift
                ;;
            --novim)
                novim=1
                shift
                ;;
            *)
                file="$1"
                shift
                ;;
        esac
    done

    if [ -z "$file" ]; then
        echo "usage: rec [--time] [--novim] filename"
        return 1
    fi

    write_mode=$(_rec_choose_write_mode "$with_time" "$file") || return 1

    if [ "$with_time" -eq 0 ]; then
        rm -f "$file.time"
    fi

    if [ "$with_time" -eq 1 ]; then
        local tmp
        local tmp_time
        local body
        local body_time
        local out
        local out_time
        local old_no_footer
        local status

        tmp=$(mktemp)
        tmp_time=$(mktemp)
        body=$(mktemp)
        body_time=$(mktemp)
        out=$(mktemp)
        out_time=$(mktemp)
        old_no_footer=$(mktemp)

        script -q -f --timing="$tmp_time" "$tmp"
        status=$?

        _rec_extract_script_body "$tmp" "$body" "$tmp_time" "$body_time"

        if [ "$novim" -eq 1 ]; then
            _rec_filter_novim "$body" "$out" "$body_time" "$out_time"
        else
            cat "$body" > "$out"
            cat "$body_time" > "$out_time"
        fi

        if [ "$write_mode" = "append" ]; then
            _rec_drop_script_footer "$file" "$old_no_footer"
            cat "$old_no_footer" > "$file"
            cat "$out" >> "$file"
            _rec_write_script_footer >> "$file"
            _rec_append_timing "$out_time" "$file.time"
        else
            _rec_write_script_header > "$file"
            cat "$out" >> "$file"
            _rec_write_script_footer >> "$file"
            cat "$out_time" > "$file.time"
        fi

        _rec_validate_timing "$file" "$file.time"

        rm -f "$tmp" "$tmp_time" "$body" "$body_time" "$out" "$out_time" "$old_no_footer"
        return "$status"
    else
        local tmp
        local tmp2
        local out
        local status
        local rec_cols

        tmp=$(mktemp)
        tmp2=$(mktemp)
        out=$(mktemp)
        rec_cols=10000

        script -q -f -c "stty cols $rec_cols 2>/dev/null; exec \"\${SHELL:-/bin/bash}\" -i" "$tmp"
        status=$?

        if [ "$novim" -eq 1 ]; then
            _rec_filter_novim "$tmp" "$tmp2"
            _rec_clean_control "$tmp2" "$out" "$rec_cols"
        else
            _rec_clean_control "$tmp" "$out" "$rec_cols"
        fi

        if [ "$write_mode" = "append" ]; then
            cat "$out" >> "$file"
        else
            cat "$out" > "$file"
        fi

        rm -f "$tmp" "$tmp2" "$out"
        return "$status"
    fi
}