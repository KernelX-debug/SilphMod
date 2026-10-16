#!/system/bin/sh
set -eu
silph_probe=0
if [ "${1:-}" = "--probe" ]; then
    silph_probe=1
    silph_parent_namespace="$2"
else
    silph_parent_namespace="$1"
    silph_binary="$2"
    silph_directory="$3"
    silph_loader="$4"
fi
silph_current_namespace="$(readlink /proc/self/ns/mnt)"
if [ "$silph_current_namespace" = "$silph_parent_namespace" ]; then
    printf '%s\n' 'SILPH_GUARD_ERROR=mount_namespace_not_isolated' >&2
    exit 90
fi
mount -t none -o rslave none /
if ! awk '($5 == "/" || $5 == "/sys/fs/selinux") && / shared:/ { unsafe = 1 } END { exit unsafe }' /proc/self/mountinfo; then
    printf '%s\n' 'SILPH_GUARD_ERROR=outbound_mount_propagation_possible' >&2
    exit 92
fi
if [ "$silph_probe" = 0 ] && [ -w /sys/fs/selinux/context ]; then
    exec 9<> /sys/fs/selinux/context
    export SILPH_LOADER_CONTEXT_FD=9
fi
if [ -d /sys/fs/selinux ]; then
    mount -t tmpfs -o ro,nosuid,nodev,noexec,size=4k silph-policy-view /sys/fs/selinux
fi
if [ -e /sys/fs/selinux/policy ] || [ -e /sys/fs/selinux/load ]; then
    printf '%s\n' 'SILPH_GUARD_ERROR=policy_path_still_visible' >&2
    exit 91
fi
if [ "$silph_probe" = 1 ]; then
    printf '%s\n' 'SILPH_GUARD_READY=isolated_mount_namespace'
    exit 0
fi
mkdir -p "$silph_directory"
export TMPDIR="$silph_directory"
if [ ! -f "$silph_loader" ]; then
    printf '%s\n' 'SILPH_GUARD_ERROR=loader_missing' >&2
    exit 93
fi
export SILPH_LOADER_SCOPE=com.nianticlabs.pokemongo
export LD_PRELOAD="$silph_loader"
printf 'ROOTCATCH_HELPER_PID=%s\n' "$$"
printf '%s\n' 'SILPH_POLICY_WRITE_GUARD=isolated_mount_namespace'
shift 4
if [ "${SILPH_LOADER_CONTEXT_FD:-}" = 9 ]; then
    exec "$silph_binary" "$@" 9<&9
else
    exec "$silph_binary" "$@"
fi
