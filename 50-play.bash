play() {
    local speed=1
    local realtime=0
    local file=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --speed)
                if [ -z "$2" ]; then
                    echo "usage: play [--speed n] [--realtime] filename"
                    return 1
                fi
                speed="$2"
                shift 2
                ;;
            --realtime)
                realtime=1
                shift
                ;;
            *)
                file="$1"
                shift
                ;;
        esac
    done

    if [ -z "$file" ]; then
        echo "usage: play [--speed n] [--realtime] filename"
        return 1
    fi

    if [ -f "$file.time" ]; then
        if [ "$realtime" -eq 1 ]; then
            scriptreplay "$file.time" "$file" "$speed"
        else
            scriptreplay -m 0.2 "$file.time" "$file" "$speed"
        fi

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