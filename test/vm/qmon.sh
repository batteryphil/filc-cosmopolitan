#!/bin/bash
# Helpers for driving a headless QEMU VM through its monitor socket.
#   qmon.sh shot <sock> <out.png>     screenshot the VGA console
#   qmon.sh type <sock> "<text>"      type text (\n = Enter)
set -eu
cmd=$1; sock=$2
mon() { echo "$1" | socat - UNIX-CONNECT:"$sock" >/dev/null; }

case $cmd in
  shot)
    out=$3; ppm=${out%.png}.ppm
    mon "screendump $ppm"; sleep 1
    python3 -c "from PIL import Image; Image.open('$ppm').save('$out')"
    rm -f "$ppm" ;;
  type)
    text=$3
    for ((i = 0; i < ${#text}; i++)); do
      c=${text:i:1}
      case "$c" in
        [a-z0-9]) k=$c ;;
        [A-Z])    k="shift-${c,,}" ;;
        ' ') k=spc ;;  '.') k=dot ;;  '/') k=slash ;;  '-') k=minus ;;
        '_') k=shift-minus ;;  '=') k=equal ;;  '>') k=shift-dot ;;
        '<') k=shift-comma ;;  '|') k=shift-backslash ;;  ';') k=semicolon ;;
        ':') k=shift-semicolon ;;  '&') k=shift-7 ;;  '*') k=shift-8 ;;
        '$') k=shift-4 ;;  '"') k=shift-apostrophe ;;  "'") k=apostrophe ;;
        ',') k=comma ;;  '(') k=shift-9 ;;  ')') k=shift-0 ;;
        $'\n') k=ret ;;
        *) echo "unsupported char: $c" >&2; exit 1 ;;
      esac
      mon "sendkey $k"; sleep 0.05
    done ;;
esac
