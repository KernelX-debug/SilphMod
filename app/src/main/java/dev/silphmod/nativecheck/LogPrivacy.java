package dev.silphmod.nativecheck;

import java.util.regex.Pattern;

final class LogPrivacy {
    private static final Pattern TEXT=Pattern.compile("(?i)(\\b(?:controlKeyLo|key_lo)=)[0-9]+");
    private static final Pattern JSON=Pattern.compile("(?i)(\"(?:controlKeyLo|key_lo)\"\\s*:\\s*)(?:\"[0-9]+\"|[0-9]+)");
    static String redact(String value){
        if(value==null)return "";
        return JSON.matcher(TEXT.matcher(value).replaceAll("$1[redacted]")).replaceAll("$1null");
    }
}
