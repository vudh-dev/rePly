#!/bin/sh
set -eu
state=/home/root/.local/share/reply
sh "$state/check-tablet-runtime.sh"
runtime=/home/root/reply-runtime
hashroot=/home/root/reply-hash-runtime
rm -f "$state/rebuild.ok" "$state/rebuild.failed"
mkdir -p "$hashroot/extensions.d" "$hashroot/exthome/qt-resource-rebuilder"
ln -sf "$runtime/extensions.d/qt-resource-rebuilder.so" "$hashroot/extensions.d/qt-resource-rebuilder.so"
# Stop any existing session. On failure restore normal service startup.
trap 'test -z "${job:-}" || kill "$job" 2>/dev/null || true; pidof xochitl >/dev/null && kill $(pidof xochitl) || true; touch "$state/rebuild.failed"; systemctl start xochitl' EXIT
systemctl stop xochitl
rm -f "$runtime/exthome/qt-resource-rebuilder/hashtab"
timeout 180 env XOVI_ROOT="$hashroot" QMLDIFF_HASHTAB_CREATE="$runtime/exthome/qt-resource-rebuilder/hashtab" QML_DISABLE_DISK_CACHE=1 LD_PRELOAD="$runtime/xovi.so" /usr/bin/xochitl > "$state/hash-output.log" 2>&1 &
job=$!
for n in $(seq 1 85); do
    if [ -s "$runtime/exthome/qt-resource-rebuilder/hashtab" ] && grep -q 'Hashtab saved to' "$state/hash-output.log"; then
        kill "$job" 2>/dev/null || true
        # The service is stopped, so this is only our indexing process.
        pidof xochitl >/dev/null && kill $(pidof xochitl) || true
        wait "$job" 2>/dev/null || true
        touch "$state/rebuild.ok"
        trap - EXIT
        exit 0
    fi
    kill -0 "$job" 2>/dev/null || exit 1
    sleep 2
done
kill "$job" 2>/dev/null || true
exit 1
