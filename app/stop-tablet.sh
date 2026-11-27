#!/bin/sh
set -eu
# Remove only this app's temporary override. Existing installations remain intact.
override=/run/systemd/system/xochitl.service.d/90-reply.conf
[ -f "$override" ] || exit 0
rm -f "$override"
systemctl daemon-reload
systemctl restart xochitl
