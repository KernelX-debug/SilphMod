package dev.rootcatch.ugphone;

import dev.rootcatch.nativeengine.EnginePolicy;
import java.util.LinkedHashMap;
import java.util.Map;

/** Fixed, read-only sources. No commands, paths or package names from log data. */
final class DiagnosticCommands {
    private DiagnosticCommands() { }
    static String bounded(String command, int seconds) {
        if (seconds < 1 || seconds > 240) throw new IllegalArgumentException("Invalid timeout");
        return "rc_ds=$(cat /proc/$$/stat); rc_dt=${rc_ds##*) }; set -- $rc_dt; "
                + "printf 'RC_DIAG_OWNER:%s:%s\\n' \"$$\" \"${20}\"; "
                + "if command -v timeout >/dev/null 2>&1; then exec timeout -s TERM " + seconds + " sh -c " + EnginePolicy.quote(command)
                + "; elif /system/bin/toybox timeout --help >/dev/null 2>&1; then exec /system/bin/toybox timeout -s TERM " + seconds
                + " sh -c " + EnginePolicy.quote(command) + "; else echo RC_NATIVE_TIMEOUT_UNAVAILABLE; exit 125; fi";
    }
    static String liveLogcat() { return "exec logcat -b all -v threadtime -T 1 '*:V'"; }
    static String process(int pid, long ticks, boolean maps) {
        if (pid <= 1 || ticks <= 0) throw new IllegalArgumentException("Invalid identity");
        String base = "/proc/" + pid;
        return "rc_ps=$(cat " + base + "/stat 2>/dev/null); rc_pt=${rc_ps##*) }; set -- $rc_pt; "
                + "printf 'EXPECTED_START=" + ticks + "\\nACTUAL_START=%s\\n' \"${20}\"; "
                + "if [ \"${20}\" != '" + ticks + "' ]; then echo RC_PROCESS_CHANGED_OR_GONE; exit 3; fi; "
                + "cat " + base + "/stat " + base + "/status " + base + "/comm; "
                + "tr '\\000' '\\n' < " + base + "/cmdline; "
                + (maps ? "cat " + base + "/maps; " : "")
                + "rc_n=0; for rc_t in " + base + "/task/[0-9]*; do "
                + "rc_n=$((rc_n+1)); [ \"$rc_n\" -le 256 ] || { echo RC_THREAD_LIMIT; break; }; "
                + "printf '\\nTHREAD %s\\n' \"${rc_t##*/}\"; cat \"$rc_t/comm\" \"$rc_t/wchan\"; done";
    }
    static Map<String, String> snapshots(int pid, long ticks) {
        Map<String, String> commands = new LinkedHashMap<>();
        commands.put("logcat", "logcat -d -b all -v threadtime -t 12000 '*:V'");
        if (pid > 1 && ticks > 0) commands.put("game-process", process(pid, ticks, true));
        commands.put("process-list", "ps -A -T; ps -A; cat /proc/meminfo; cat /proc/uptime");
        commands.put("activity", "dumpsys -t 4 activity processes; dumpsys -t 4 activity exit-info com.nianticlabs.pokemongo");
        commands.put("window", "dumpsys -t 4 window windows");
        commands.put("kernel", "dmesg");
        commands.put("dropbox-native", "dumpsys -t 4 dropbox --print data_app_native_crash");
        commands.put("dropbox-java", "dumpsys -t 4 dropbox --print data_app_crash");
        commands.put("tools", "id; date; uname -a; getenforce; command -v timeout; timeout --help; logcat -g; logcat --help; "
                + "getprop ro.build.fingerprint; getprop ro.build.version.release; getprop ro.product.cpu.abilist");
        return commands;
    }
    static String tombstoneInventory() {
        // Text tombstones only, no protobufs or files outside the fixed directory.
        return "for rc_f in /data/tombstones/tombstone_*; do [ -f \"$rc_f\" ] || continue; "
                + "[ ! -L \"$rc_f\" ] || continue; rc_b=${rc_f##*/}; "
                + "case \"$rc_b\" in *.pb|*[^a-z0-9_]*) continue;; esac; "
                + "head -c 8192 \"$rc_f\" | grep -E '^Cmdline: com[.]nianticlabs[.]pokemongo$|>>> com[.]nianticlabs[.]pokemongo <<<' >/dev/null || continue; "
                + "printf '%s ' \"$rc_b\"; stat -c '%i %s %Y' \"$rc_f\"; done";
    }
    static String tombstone(String basename) {
        if (!basename.matches("tombstone_[0-9]{2,4}")) throw new IllegalArgumentException("Tombstone not allowed");
        String file = "/data/tombstones/" + basename;
        return "[ -f '" + file + "' ] && [ ! -L '" + file + "' ] || exit 4; "
                + "exec 3< '" + file + "'; [ \"$(readlink /proc/$$/fd/3)\" = '" + file + "' ] || exit 4; "
                + "head -c 8192 '" + file + "' | grep -E '^Cmdline: com[.]nianticlabs[.]pokemongo$|>>> com[.]nianticlabs[.]pokemongo <<<' >/dev/null || exit 4; "
                + "cat <&3";
    }
    static String stopOwned(int pid, long ticks) {
        if (pid <= 1 || ticks <= 0) throw new IllegalArgumentException("Invalid helper");
        String base = "/proc/" + pid;
        return "rc_ks=$(cat " + base + "/stat 2>/dev/null); rc_kt=${rc_ks##*) }; set -- $rc_kt; "
                + "[ \"${20}\" = '" + ticks + "' ] || exit 3; "
                + "rc_kn=$(tr '\\000' '\\n' < " + base + "/cmdline | head -n 1); "
                + "case \"$rc_kn\" in timeout|/system/bin/timeout) ;; /system/bin/toybox) "
                + "[ \"$(tr '\\000' '\\n' < " + base + "/cmdline | head -n 2 | tail -n 1)\" = timeout ] || exit 4;; *) exit 4;; esac; "
                + "for rc_kc in $(cat " + base + "/task/" + pid + "/children 2>/dev/null); do "
                + "rc_cs=$(cat /proc/$rc_kc/stat 2>/dev/null); rc_ct=${rc_cs##*) }; set -- $rc_ct; "
                + "[ \"$2\" = '" + pid + "' ] || continue; "
                + "rc_cn=$(tr '\\000' '\\n' < /proc/$rc_kc/cmdline | head -n 1); "
                + "case \"$rc_cn\" in logcat|/system/bin/logcat) kill -TERM \"$rc_kc\";; esac; done; "
                + "kill -TERM " + pid;
    }
}
