package dev.rootcatch.nativeengine;

import java.io.Closeable;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/** Complete UTF-8 records, bounded storage; archive 1 is the newest previous segment. */
public final class RollingLog implements Closeable {
    private final File directory;
    private final String name;
    private final long limit;
    private final int archives;
    private FileOutputStream output;
    private long bytes, totalBytes, rotations;

    public RollingLog(File directory, String name, long limit, int archives) throws IOException {
        if (!name.matches("[a-z][a-z-]*\\.(jsonl|log)") || limit < 1 || archives < 0 || archives > 2)
            throw new IllegalArgumentException("Invalid log budget/name");
        this.directory = directory; this.name = name; this.limit = limit; this.archives = archives;
        output = new FileOutputStream(new File(directory, name));
    }
    public synchronized void write(String record) throws IOException {
        if (output == null) throw new IOException("Log closed");
        byte[] data = (record + "\n").getBytes(StandardCharsets.UTF_8);
        if (data.length > limit) throw new IOException("Record exceeds log segment budget");
        if (bytes + data.length > limit) {
            output.close(); rotate(directory, name, archives);
            output = new FileOutputStream(new File(directory, name)); bytes = 0; rotations++;
        }
        output.write(data); output.flush(); bytes += data.length; totalBytes += data.length;
    }
    public static String archiveName(String name, int index) {
        int dot = name.lastIndexOf('.');
        return name.substring(0, dot) + "-" + index + name.substring(dot);
    }
    public static void rotate(File directory, String name, int archives) throws IOException {
        for (int i = archives; i >= 1; i--) {
            File source = new File(directory, i == 1 ? name : archiveName(name, i - 1));
            if (source.isFile()) Files.move(source.toPath(), new File(directory, archiveName(name, i)).toPath(),
                    StandardCopyOption.REPLACE_EXISTING);
        }
    }
    public synchronized long rotations() { return rotations; }
    public synchronized long totalBytes() { return totalBytes; }
    @Override public synchronized void close() throws IOException {
        if (output != null) { output.close(); output = null; }
    }
}
