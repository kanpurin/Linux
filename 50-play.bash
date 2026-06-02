play() {
    local speed=1
    local realtime=0
    local force=0
    local file=""
    local warn
    local ans
    local clean_time

    while [ $# -gt 0 ]; do
        case "$1" in
            --speed)
                if [ -z "$2" ]; then
                    echo "usage: play [--speed n] [--realtime] [--force] filename"
                    return 1
                fi
                speed="$2"
                shift 2
                ;;
            --realtime)
                realtime=1
                shift
                ;;
            --force)
                force=1
                shift
                ;;
            *)
                file="$1"
                shift
                ;;
        esac
    done

    if [ -z "$file" ]; then
        echo "usage: play [--speed n] [--realtime] [--force] filename"
        return 1
    fi

    if [ -f "$file.time" ]; then
        _rec_check_terminal_size_for_replay "$file.time"
        warn=$?

        if [ "$warn" -ne 0 ] && [ "$force" -ne 1 ]; then
            printf "continue? [y/N]: " >&2
            read -r ans

            case "$ans" in
                y|Y|yes|YES)
                    :
                    ;;
                *)
                    echo "play: canceled" >&2
                    return 1
                    ;;
            esac
        fi

        clean_time=$(mktemp)
        _rec_strip_timing_meta "$file.time" "$clean_time"

        if [ "$realtime" -eq 1 ]; then
            scriptreplay "$clean_time" "$file" "$speed"
        else
            scriptreplay -m 0.2 "$clean_time" "$file" "$speed"
        fi

        rm -f "$clean_time"

        local oldstty
        oldstty=$(stty -g)
        stty -echo -icanon time 1 min 0
        while IFS= read -r -s -n 1024 -t 0.05 _junk; do
            :
        done
        stty "$oldstty"

        printf '\r\033[K\n'
    else
        cat "$file"
    fi
}