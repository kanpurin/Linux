_rec_append_timing() {
    local src_time="$1"
    local dst_time="$2"
    local tmp

    tmp=$(mktemp)

    if [ -f "$dst_time" ]; then
        _rec_strip_timing_meta "$dst_time" "$tmp"
        cat "$tmp" > "$dst_time"
    fi

    rm -f "$tmp"

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
    local clean_time
    local body_size
    local time_size

    [ -f "$log_file" ] || return 1
    [ -f "$time_file" ] || return 1

    clean_time=$(mktemp)
    _rec_strip_timing_meta "$time_file" "$clean_time"

    body_size=$(_rec_replay_body_size "$log_file")
    time_size=$(_rec_timing_size "$clean_time")

    rm -f "$clean_time"

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
        local rec_size
        local rec_rows
        local rec_cols
        local old_meta
        local old_rows
        local old_cols
        local old_mixed
        local meta_rows
        local meta_cols
        local size_mixed=0

        tmp=$(mktemp)
        tmp_time=$(mktemp)
        body=$(mktemp)
        body_time=$(mktemp)
        out=$(mktemp)
        out_time=$(mktemp)
        old_no_footer=$(mktemp)

        rec_size=$(_rec_current_terminal_size)
        if [ $? -eq 0 ]; then
            set -- $rec_size
            rec_rows="$1"
            rec_cols="$2"
            meta_rows="$rec_rows"
            meta_cols="$rec_cols"
        else
            echo "rec: warning: failed to get terminal size metadata" >&2
            rec_rows=""
            rec_cols=""
            meta_rows=""
            meta_cols=""
            size_mixed=1
        fi

        if [ "$write_mode" = "append" ]; then
            old_meta=$(_rec_load_timing_meta "$file.time")
            if [ $? -eq 0 ]; then
                set -- $old_meta
                old_rows="$1"
                old_cols="$2"
                old_mixed="$3"

                meta_rows="$old_rows"
                meta_cols="$old_cols"

                if [ "$old_mixed" = "1" ]; then
                    size_mixed=1
                fi

                if [ -n "$rec_rows" ] && [ -n "$rec_cols" ]; then
                    if [ "$old_rows" != "$rec_rows" ] || [ "$old_cols" != "$rec_cols" ]; then
                        echo "rec: warning: appending with different terminal size: existing=${old_rows}x${old_cols} current=${rec_rows}x${rec_cols}" >&2
                        echo "rec: warning: replay may be broken" >&2
                        size_mixed=1
                    fi
                else
                    size_mixed=1
                fi
            else
                echo "rec: warning: existing terminal size metadata is missing" >&2
                echo "rec: warning: replay size check may be incomplete" >&2
                size_mixed=1

                if [ -n "$rec_rows" ] && [ -n "$rec_cols" ]; then
                    meta_rows="$rec_rows"
                    meta_cols="$rec_cols"
                fi
            fi
        fi

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

        if [ -n "$meta_rows" ] && [ -n "$meta_cols" ]; then
            _rec_write_timing_meta "$file.time" "$meta_rows" "$meta_cols" "$size_mixed"
        else
            echo "rec: warning: terminal size metadata was not saved" >&2
        fi

        rm -f "$tmp" "$tmp_time" "$body" "$body_time" "$out" "$out_time" "$old_no_footer"
        return "$status"
    else
        local tmp
        local tmp2
        local out
        local status
        local cols

        tmp=$(mktemp)
        tmp2=$(mktemp)
        out=$(mktemp)
        cols=$(stty size 2>/dev/null | awk '{print $2}')
        [ -n "$cols" ] || cols=80

        script -q -f -c 'exec "${SHELL:-/bin/bash}" -i' "$tmp"
        status=$?

        if [ "$novim" -eq 1 ]; then
            _rec_filter_novim "$tmp" "$tmp2"
            _rec_clean_control "$tmp2" "$out" "$cols"
        else
            _rec_clean_control "$tmp" "$out" "$cols"
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