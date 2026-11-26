#!/bin/sh
set -eu
# Remove only this app's temporary override. Existing installations remain intact.
rm -f /run/systemd/system/xochitl.service.d/90-reply.conf
systemctl daemon-reload
systemctl restart xochitl
