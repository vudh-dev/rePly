#!/bin/sh
set -eu
umask 077
stage=/home/root/reply-setup
runtime=/home/root/reply-runtime
state=/home/root/.local/share/reply
sh "$stage/check-tablet-runtime.sh"
# Runtime is isolated from any existing XOVI / Ink Diary installation.
mkdir -p "$state" "$runtime" "$stage/unpack"
if [ -d /home/root/.config/reply ]; then
    tar -czf "$state/config-before-install.tar.gz" -C /home/root/.config reply
fi
tar -xzf "$stage/xovi-aarch64.tar.gz" -C "$stage/unpack"
upstream="$stage/unpack/xovi"
mkdir -p "$runtime/extensions.d" "$runtime/exthome/qt-resource-rebuilder"
atomic_copy() { cp "$1" "$2.new"; mv "$2.new" "$2"; }
atomic_copy "$upstream/xovi.so" "$runtime/xovi.so"
atomic_copy "$upstream/extensions.d/qt-resource-rebuilder.so" "$runtime/extensions.d/qt-resource-rebuilder.so"
for module in xovi-message-broker framebuffer-spy; do
    atomic_copy "$upstream/inactive-extensions/$module.so" "$runtime/extensions.d/$module.so"
done
atomic_copy "$stage/rm-shot-aarch64.so" "$runtime/extensions.d/rm-shot.so"
tar -xzf "$stage/tablet-payload.tar.gz" -C "$stage/unpack"
cp "$stage/unpack/payload/reply.so" "$runtime/extensions.d/reply.so.new"
chmod 755 "$runtime/extensions.d/reply.so.new"
mv "$runtime/extensions.d/reply.so.new" "$runtime/extensions.d/reply.so"
cp "$stage/unpack/payload/reply.qmd" "$runtime/exthome/qt-resource-rebuilder/"
mkdir -p /home/root/.config/reply
cp "$stage/unpack/payload/"*.ttf /home/root/.config/reply/
cp "$stage/unpack/payload/"*.svg /home/root/.config/reply/
# Any previous capture proof belongs to an earlier firmware/session.
rm -f /home/root/.config/reply/fast-capture-verified
cp "$stage/unpack/payload/"*.sh "$state/"
cp "$stage/check-tablet-runtime.sh" "$state/check-tablet-runtime.sh"
chmod 700 "$state/"*.sh
