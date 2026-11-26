#!/bin/sh
set -eu
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
