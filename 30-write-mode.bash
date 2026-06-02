_rec_choose_write_mode() {
    local wt="$1"
    local file="$2"
    local found=0
    local appendable=0
    local ans

    if [ "$wt" != "0" ] && [ "$wt" != "1" ]; then
        echo "internal error: wt must be 0 or 1: $wt" >&2
        return 1
    fi

    if [ -e "$file" ]; then
        printf "file exists: %s\n" "$file" >&2
        found=1
    fi

    if [ "$wt" -eq 1 ] && [ -e "$file.time" ]; then
        printf "file exists: %s\n" "$file.time" >&2
        found=1
    fi

    if [ "$wt" -eq 0 ] && [ -e "$file.time" ]; then
        printf "timing file exists and will be removed: %s\n" "$file.time" >&2
        found=1
    fi

    if [ "$wt" -eq 1 ]; then
        if [ -e "$file" ] && [ -e "$file.time" ]; then
            appendable=1
        fi
    else
        if [ -e "$file" ]; then
            appendable=1
        fi
    fi

    if [ "$found" -eq 0 ]; then
        echo "overwrite"
        return 0
    fi

    while true; do
        if [ "$appendable" -eq 1 ]; then
            printf "select [o]verwrite / [a]ppend / [c]ancel: " >&2
        else
            printf "select [o]verwrite / [c]ancel: " >&2
        fi

        read -r ans

        case "$ans" in
            o|O|overwrite|OVERWRITE|y|Y|yes|YES)
                echo "overwrite"
                return 0
                ;;
            a|A|append|APPEND)
                if [ "$appendable" -eq 1 ]; then
                    echo "append"
                    return 0
                fi
                printf "append is not available\n" >&2
                ;;
            ""|c|C|cancel|CANCEL|n|N|no|NO)
                printf "canceled\n" >&2
                return 1
                ;;
            *)
                printf "invalid selection\n" >&2
                ;;
        esac
    done
}