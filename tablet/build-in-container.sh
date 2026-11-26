#!/usr/bin/env bash
set -euo pipefail
sdk=/opt/codex/ferrari/5.8.203
if ! compgen -G "$sdk/environment-setup-*" >/dev/null; then
    bash /downloads/ferrari-3.28-aarch64.sh -y -d "$sdk"
fi
export SDK_ENV
SDK_ENV=$(compgen -G "$sdk/environment-setup-*" | head -n 1)
export XOVI_REPO=/xovi
bash build.sh
file bin/reply.so bin/config-model
sha256sum bin/reply.so bin/config-model > bin/SHA256SUMS
