#!/bin/sh
# Runs the filc-cosmopolitan APE test payload on the current OS.
# POSIX sh only (works on Linux and the BSDs); has its own timeout because
# not every BSD ships timeout(1).  Output is parsed by summarize.sh.
cd "$(dirname "$0")" || exit 1
chmod +x ./ape_*.com 2>/dev/null

LIMIT=60

run() {
  name=$1; shift
  echo "=== BEGIN $name"
  "$@" > out.$$ 2>&1 &
  pid=$!
  waited=0
  while kill -0 "$pid" 2>/dev/null; do
    if [ "$waited" -ge "$LIMIT" ]; then
      kill -9 "$pid" 2>/dev/null
      echo "TIMEOUT after ${LIMIT}s" >> out.$$
      break
    fi
    sleep 1
    waited=$((waited + 1))
  done
  wait "$pid" 2>/dev/null
  rc=$?
  cat out.$$
  rm -f out.$$
  echo "=== END $name rc=$rc"
}

echo "### OS: $(uname -srm 2>/dev/null)"
# OpenBSD's ksh rejects the APE shell-script header ("NUL byte unexpected"),
# so there every binary is launched through Cosmopolitan's standalone APE
# loader instead (applies equally to the control).
L=""
if [ "$(uname -s)" = OpenBSD ] && [ -f ./ape-loader ]; then
  chmod +x ./ape-loader; command -v elfctl >/dev/null 2>&1 && elfctl -e +wx ./ape-loader 2>/dev/null || true; L="./ape-loader"
  echo "### launching via APE loader"
fi
# Control: an official upstream Cosmopolitan APE with no Fil-C in it.  If this
# fails too, the problem is the APE format/loader on this OS, not Fil-C.
run control      sh -c "rm -f ctl_touched; $L ./ctl_touch.ape ctl_touched && [ -f ctl_touched ] && echo CONTROL_OK"
run smoke        $L ./ape_portable_smoke.com
run stress1      $L ./ape_stress_test.com 1
run stress2      $L ./ape_stress_test.com 2
run stress3      $L ./ape_stress_test.com 3
run stress4      $L ./ape_stress_test.com 4
run malloc_align $L ./ape_malloc_align.com
run ifconf_ok    $L ./ape_ioctl_ifconf_ok.com
run ifconf_bad   $L ./ape_ioctl_nested_ptr.com
echo "### DONE"
