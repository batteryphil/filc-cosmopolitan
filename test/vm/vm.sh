#!/bin/bash
# filc-cosmopolitan multi-OS VM harness.
#
#   vm.sh fetch <os>     download + verify base image into $VMROOT/images
#   vm.sh test  <os>     boot a throwaway overlay, copy payload, run tests,
#                        save $VMROOT/results/<os>.log, power off
#   vm.sh list           show known OS names
#
# Base images are never modified: every run uses a fresh qcow2 overlay.
# Access is via a throwaway SSH key injected with a cloud-init NoCloud seed.
set -u
VMROOT=${VMROOT:-/data/filc-vm}
IMG=$VMROOT/images
RUN=$VMROOT/run
RES=$VMROOT/results
PAYLOAD=$VMROOT/payload
KEY=$VMROOT/id_ed25519
mkdir -p "$IMG" "$RUN" "$RES"

# name | url | file-in-images | firmware | default-user | ssh-port
os_table() {
cat <<'EOF'
debian12|https://cloud.debian.org/images/cloud/bookworm/latest/debian-12-genericcloud-amd64.qcow2|debian-12.qcow2|bios|debian|2201
fedora44|https://dl.fedoraproject.org/pub/fedora/linux/releases/44/Cloud/x86_64/images/Fedora-Cloud-Base-Generic-44-1.7.x86_64.qcow2|fedora-44.qcow2|bios|fedora|2202
alpine323|https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/cloud/nocloud_alpine-3.23.4-x86_64-uefi-cloudinit-r0.qcow2|alpine-3.23.qcow2|uefi|alpine|2203
centos7|https://cloud.centos.org/centos/7/images/CentOS-7-x86_64-GenericCloud-2211.qcow2c|centos-7.qcow2|bios|centos|2204
freebsd14|https://download.freebsd.org/releases/VM-IMAGES/14.5-RELEASE/amd64/Latest/FreeBSD-14.5-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2.xz|freebsd-14.5.qcow2|bios|freebsd|2205
freebsd15|https://download.freebsd.org/releases/VM-IMAGES/15.1-RELEASE/amd64/Latest/FreeBSD-15.1-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2.xz|freebsd-15.1.qcow2|bios|freebsd|2206
netbsd10|https://object-storage.public.mtl1.vexxhost.net/swift/v1/1dbafeefbd4f4c80864414a441e72dd2/bsd-cloud-image.org/images/netbsd/10.1/2025-02-15/ufs/netbsd-10.1-2025-02-15.qcow2|netbsd-10.1.qcow2|bios|netbsd|2207
openbsd78|https://github.com/hcartiaux/openbsd-cloud-image/releases/download/v7.8_2025-10-22-09-25/openbsd-min.qcow2|openbsd-7.8.qcow2|bios|openbsd|2208
EOF
}

lookup() {
  local row
  row=$(os_table | awk -F'|' -v n="$1" '$1==n')
  [ -z "$row" ] && { echo "unknown os: $1 (try: vm.sh list)" >&2; exit 2; }
  IFS='|' read -r NAME URL FILE FW DUSER PORT <<<"$row"
}

fetch() {
  lookup "$1"
  local dst=$IMG/$FILE
  if [ -s "$dst" ]; then echo "[$NAME] already have $dst"; return 0; fi
  echo "[$NAME] downloading $URL"
  local tmp=$dst.part
  curl -fL --retry 3 -o "$tmp" "$URL" || { echo "[$NAME] download FAILED"; rm -f "$tmp"; return 1; }
  case $URL in
    *.xz) mv "$tmp" "$tmp.xz" && xz -d -T0 "$tmp.xz" || return 1 ;;
  esac
  # Validate it's really a disk image (catches HTML error pages etc.)
  qemu-img info "$tmp" >/dev/null 2>&1 || { echo "[$NAME] not a valid image"; return 1; }
  mv "$tmp" "$dst"
  echo "[$NAME] OK $(du -h "$dst" | cut -f1) sha256=$(sha256sum "$dst" | cut -c1-16)…"
}

make_seed() {  # $1 = run dir
  [ -f "$KEY" ] || ssh-keygen -q -t ed25519 -N '' -f "$KEY" -C filc-vm-throwaway
  local pub; pub=$(cat "$KEY.pub")
  cat > "$1/user-data" <<EOF
#cloud-config
ssh_pwauth: false
ssh_authorized_keys:
  - $pub
users:
  - default
  - name: filc
    shell: /bin/sh
    sudo: ALL=(ALL) NOPASSWD:ALL
    ssh_authorized_keys:
      - $pub
EOF
  printf 'instance-id: filc-%s\nlocal-hostname: filc-%s\n' "$NAME" "$NAME" > "$1/meta-data"
  genisoimage -quiet -output "$1/seed.iso" -volid cidata -joliet -rock \
    "$1/user-data" "$1/meta-data"
}

SSHOPTS=(-i "$KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
         -o ConnectTimeout=5 -o LogLevel=ERROR -o BatchMode=yes)

try_users() {  # find a login that works; prints the user
  local u
  for u in filc "$DUSER" root; do
    if ssh "${SSHOPTS[@]}" -p "$PORT" "$u@127.0.0.1" true 2>/dev/null; then
      echo "$u"; return 0
    fi
  done
  return 1
}

test_os() {
  lookup "$1"
  [ -s "$IMG/$FILE" ] || fetch "$1" || return 1
  local d=$RUN/$NAME; rm -rf "$d"; mkdir -p "$d"
  qemu-img create -q -f qcow2 -F qcow2 -b "$IMG/$FILE" "$d/disk.qcow2" 20G
  make_seed "$d"
  local fwargs=()
  if [ "$FW" = uefi ]; then
    cp /usr/share/OVMF/OVMF_VARS_4M.fd "$d/vars.fd"
    fwargs=(-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd
            -drive if=pflash,format=raw,file="$d/vars.fd")
  fi
  echo "[$NAME] booting (ssh port $PORT, console log $d/console.log)"
  qemu-system-x86_64 -enable-kvm -cpu host -smp 2 -m 2048 "${fwargs[@]}" \
    -drive file="$d/disk.qcow2",if=virtio,format=qcow2 \
    -drive file="$d/seed.iso",media=cdrom,format=raw \
    -netdev user,id=n0,hostfwd=tcp:127.0.0.1:"$PORT"-:22 -device virtio-net-pci,netdev=n0 \
    -display none -serial file:"$d/console.log" -daemonize -pidfile "$d/qemu.pid" \
    || { echo "[$NAME] qemu failed to start"; return 1; }

  local user="" i
  for i in $(seq 1 90); do  # up to ~7.5 min for first boot + cloud-init
    sleep 5
    user=$(try_users) && break
  done
  if [ -z "$user" ]; then
    echo "[$NAME] SSH never came up; see $d/console.log"
    { echo "### OS: $NAME (no SSH)"; echo "### HARNESS: ssh never came up"; } > "$RES/$NAME.log"
    kill "$(cat "$d/qemu.pid")" 2>/dev/null; return 1
  fi
  echo "[$NAME] logged in as $user"
  ssh "${SSHOPTS[@]}" -p "$PORT" "$user@127.0.0.1" 'mkdir -p filc_payload'
  scp -q "${SSHOPTS[@]}" -P "$PORT" "$PAYLOAD"/* "$user@127.0.0.1:filc_payload/" \
    || echo "[$NAME] scp failed"
  ssh "${SSHOPTS[@]}" -p "$PORT" "$user@127.0.0.1" 'ulimit -c 0; sh filc_payload/run_tests.sh' \
    > "$RES/$NAME.log" 2>&1
  echo "[$NAME] tests finished -> $RES/$NAME.log"
  ssh "${SSHOPTS[@]}" -p "$PORT" "$user@127.0.0.1" \
    'sudo poweroff 2>/dev/null || sudo shutdown -p now 2>/dev/null || doas halt -p' >/dev/null 2>&1
  sleep 10
  kill "$(cat "$d/qemu.pid")" 2>/dev/null
  return 0
}

case "${1:-}" in
  list)  os_table | cut -d'|' -f1 ;;
  fetch) shift; for o in "$@"; do fetch "$o"; done ;;
  test)  shift; for o in "$@"; do test_os "$o"; done ;;
  *)     echo "usage: $0 {list|fetch <os>...|test <os>...}"; exit 2 ;;
esac
