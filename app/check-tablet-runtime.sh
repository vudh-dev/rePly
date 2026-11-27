#!/bin/sh
# Read-only. Do not print the service environment: it may contain secrets.
set -eu

reject() {
    printf '%s\n' "$1" >&2
    exit 1
}

if [ "$#" = 0 ]; then
    [ "$(uname -m)" = aarch64 ] || reject 'Unsupported architecture'
    [ "$(tr -d '\000' < /sys/firmware/devicetree/base/model)" = 'reMarkable Ferrari' ] ||
        reject 'Unsupported hardware'
    grep -Eq '^IMG_VERSION="(3\.29\.0\.153|3\.29\.1\.0)"$' /etc/os-release ||
        reject 'Unsupported firmware'
elif [ "$#" != 1 ] || [ "$1" != '--inspect' ]; then
    reject 'Unknown preflight mode'
fi

own=/home/root/reply-runtime
pid=$(systemctl show xochitl -p MainPID --value)
case "$pid" in ''|*[!0-9]*|0|1) reject 'Cannot verify the running tablet interface' ;; esac

# A manager may have staged a new launch policy without restarting xochitl.
# Check both the startup policy and the running process, then recheck its PID.
unit=$(systemctl show xochitl -p Environment --value)
env_files=$(systemctl show xochitl -p EnvironmentFiles --value)
exec_start=$(systemctl show xochitl -p ExecStart --value)
[ -z "$env_files" ] || reject 'Cannot verify a tablet interface with external environment files'
case "$exec_start" in
    '{ path=/usr/bin/xochitl ; '*) ;;
    *) reject 'Cannot verify a non-stock tablet interface launch command' ;;
esac
[ -r "/proc/$pid/environ" ] && [ -r "/proc/$pid/maps" ] ||
    reject 'Cannot read tablet interface runtime state'

live_env=$(tr '\000' '\n' < "/proc/$pid/environ")
live=$(printf '%s\n' "$live_env" | grep -E '^(XOVI_ROOT|LD_PRELOAD)=' || :)
maps=$(grep -E '/(lib)?xovi[^/ ]*\.so|/qt-resource-rebuilder\.so' "/proc/$pid/maps" || :)
unit_owner=$(printf '%s\n' "$unit" | awk '
    {
        for (i=1; i<=NF; ++i) {
            if ($i ~ /^(XOVI_ROOT|LD_PRELOAD)=/) print $i
            else if ($i ~ /(XOVI_ROOT|LD_PRELOAD)=/) print "[unrecognized ownership setting]"
        }
    }')

check_owner() {
    # Only a complete, exact rePly loader/root pair is ours.
    [ -z "$1" ] || [ "$1" = "XOVI_ROOT=$own
LD_PRELOAD=$own/xovi.so" ] || [ "$1" = "LD_PRELOAD=$own/xovi.so
XOVI_ROOT=$own" ]
}
check_owner "$unit_owner" && check_owner "$live" ||
    reject 'Another XOVI or preload setup owns the tablet interface; rePly will not replace it'
if [ -n "$maps" ]; then
    [ -n "$live" ] && ! printf '%s\n' "$maps" | grep -Fv " $own/" >/dev/null ||
        reject 'Another loader is mapped in the tablet interface; rePly will not replace it'
fi
[ "$(systemctl show xochitl -p MainPID --value)" = "$pid" ] ||
    reject 'Tablet interface changed during preflight; try again'
printf '%s\n' 'rePly runtime preflight passed'
