#!/bin/sh
set -eu
sh /home/root/.local/share/reply/check-tablet-runtime.sh
runtime=/home/root/reply-runtime
[ -s "$runtime/exthome/qt-resource-rebuilder/hashtab" ] || { echo 'Complete installation first'; exit 1; }
mkdir -p /run/systemd/system/xochitl.service.d
cat > /run/systemd/system/xochitl.service.d/90-reply.conf <<'UNIT'
[Service]
Environment="LD_PRELOAD=/home/root/reply-runtime/xovi.so"
Environment="XOVI_ROOT=/home/root/reply-runtime"
UNIT
systemctl daemon-reload
systemctl restart xochitl
