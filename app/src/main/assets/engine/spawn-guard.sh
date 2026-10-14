#!/system/bin/sh
set -eu
silph_parent_namespace="$1"
silph_binary="$2"
silph_directory="$3"
silph_loader="$4"
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
mount -t tmpfs -o ro,nosuid,nodev,noexec,size=4k silph-policy-view /sys/fs/selinux
if [ -e /sys/fs/selinux/policy ] || [ -e /sys/fs/selinux/load ]; then
    printf '%s\n' 'SILPH_GUARD_ERROR=policy_path_still_visible' >&2
    exit 91
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
exec "$silph_binary" "$@"
