package dev.rootcatch.ugphone;

import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

final class LogArchive {
    record Receipt(long bytes, String sha256) { }
    private LogArchive() { }
    static Receipt add(ZipOutputStream zip, String name, InputStream source) throws IOException {
        final MessageDigest digest;
        try { digest = MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException error) { throw new IllegalStateException(error); }
        zip.putNextEntry(new ZipEntry(name));
        byte[] buffer = new byte[65536]; long bytes = 0; int count;
        while ((count = source.read(buffer)) != -1) {
            if (count == 0) continue;
            zip.write(buffer, 0, count); digest.update(buffer, 0, count); bytes += count;
        }
        zip.closeEntry();
        StringBuilder hash = new StringBuilder(64);
        for (byte value : digest.digest()) {
            hash.append(Character.forDigit((value >>> 4) & 15, 16)); hash.append(Character.forDigit(value & 15, 16));
        }
        return new Receipt(bytes, hash.toString());
    }
}
