#!/bin/sh
# Turns results/<os>.log files into a grid.
#   OK      = behaved as intended (smoke: all pass; safety tests: Fil-C caught it)
#   FAIL    = ran but wrong result (smoke failures listed; safety test not caught)
#   CRASH   = died without a Fil-C safety report (signal, loader error, ...)
#   TIMEOUT = hung
cd "$(dirname "$0")/results" 2>/dev/null || cd "${1:-.}" || exit 1

verdict() {  # $1=log $2=test
  block=$(awk -v t="$2" '$0=="=== BEGIN " t {on=1; next} on && $0 ~ "^=== END " t " " {print; exit} on {print}' "$1")
  [ -z "$block" ] && { echo "-"; return; }
  rc=$(printf '%s\n' "$block" | sed -n 's/^=== END .* rc=//p')
  echo "$block" | grep -q '^TIMEOUT' && { echo "TIMEOUT"; return; }
  case $2 in
    control)
      echo "$block" | grep -q "^CONTROL_OK" && echo "OK" || echo "FAIL(rc=$rc)" ;;
    smoke)
      s=$(printf '%s\n' "$block" | sed -n 's/^SUMMARY //p')
      if [ -z "$s" ]; then echo "CRASH"; return; fi
      f=$(printf '%s\n' "$block" | sed -n 's/^FAIL \([^:]*\):.*/\1/p' | tr '\n' ',' | sed 's/,$//')
      [ -z "$f" ] && echo "OK($s)" || echo "FAIL[$f]" ;;
    stress*|ifconf_bad)
      if echo "$block" | grep -q 'thwarted a futile attempt'; then echo "OK(caught)";
      elif [ "$rc" = 0 ]; then echo "FAIL(not caught)";
      else echo "CRASH(rc=$rc)"; fi ;;
    malloc_align)
      echo "$block" | grep -q '^ALL PASSED' && echo "OK" || { [ "$rc" = 0 ] && echo "FAIL" || echo "CRASH(rc=$rc)"; } ;;
    ifconf_ok)
      echo "$block" | grep -q '^RESULT: OK' && echo "OK" || echo "FAIL(rc=$rc)" ;;
  esac
}

printf '%-14s' "OS"
for t in control smoke stress1 stress2 stress3 stress4 malloc_align ifconf_ok ifconf_bad; do printf ' | %s' "$t"; done
echo
for log in *.log; do
  printf '%-14s' "${log%.log}"
  for t in control smoke stress1 stress2 stress3 stress4 malloc_align ifconf_ok ifconf_bad; do
    printf ' | %s' "$(verdict "$log" $t)"
  done
  echo
done
