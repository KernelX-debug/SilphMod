package dev.rootcatch.ugphone;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.provider.MediaStore;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** Local-only, user-requested raw export; no upload, broad filesystem scan or external storage grant. */
final class LogExporter {
    private LogExporter() { }
    static String export(Context context) throws Exception {
        File[] reports = FarmReports.recent(context.getFilesDir());
        if (reports.length == 0) throw new IllegalStateException("No logs yet");
        String filename = "SilphMod_" + new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.ROOT).format(new Date())
                + "_" + reports[0].getName().substring(14) + ".zip";
        ContentValues values = new ContentValues();
        values.put(MediaStore.Downloads.DISPLAY_NAME, filename);
        values.put(MediaStore.Downloads.MIME_TYPE, "application/zip");
        values.put(MediaStore.Downloads.RELATIVE_PATH, "Download/SilphMod");
        values.put(MediaStore.Downloads.IS_PENDING, 1);
        Uri uri = context.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        if (uri == null) throw new IllegalStateException("Could not create ZIP");
        try {
            JSONArray index = new JSONArray();
            try (OutputStream stream = context.getContentResolver().openOutputStream(uri);
                 ZipOutputStream zip = new ZipOutputStream(require(stream))) {
                for (File report : reports) {
                    for (File file : FarmReports.files(report)) {
                        String path = "sessions/" + report.getName() + "/" + file.getName();
                        String digest;
                        try (InputStream source = new FileInputStream(file)) { digest = FarmSession.hash(source); }
                        index.put(new JSONObject().put("path", path).put("bytes", file.length()).put("sha256", digest));
                        try (InputStream source = new FileInputStream(file)) { entry(zip, path, source); }
                    }
                }
                JSONObject manifest = new JSONObject().put("build", FarmPolicy.BUILD).put("appPackage", context.getPackageName())
                        .put("expectedAgentSha256", FarmPolicy.EXPECTED_AGENT_SHA).put("exportTime", System.currentTimeMillis())
                        .put("sessionCount", reports.length).put("rawLogs", true).put("files", index);
                entry(zip, "bundle-index.json", manifest.toString(2));
                entry(zip, "README.txt", "SilphMod " + FarmPolicy.BUILD + "\n"
                        + "Last " + reports.length + " local sessions. Manual export; no uploads.\n"
                        + "Unredacted logs may contain coordinates, IDs and other app data. Review before sharing.\n"
                        + "Rotated logs and time/size-limited sources: sources.jsonl records omissions and truncation.\n"
                        + "report-status.json summarizes confirmed results; latest.jsonl contains engine events; health.jsonl contains health samples.\n"
                        + "The game is not restarted and pending actions are not resumed after a process exit.\n");
                try (InputStream source = context.getAssets().open("engine/injector.json")) { entry(zip, "resources/injector.json", source); }
            }
            values.clear(); values.put(MediaStore.Downloads.IS_PENDING, 0);
            if (context.getContentResolver().update(uri, values, null, null) != 1)
                throw new IllegalStateException("Could not finish ZIP");
            return "Descargas/SilphMod/" + filename;
        } catch (Exception error) {
            context.getContentResolver().delete(uri, null, null);
            throw error;
        }
    }
    private static OutputStream require(OutputStream stream) {
        if (stream == null) throw new IllegalStateException("Could not open ZIP");
        return stream;
    }
    private static void entry(ZipOutputStream zip, String name, String text) throws Exception {
        zip.putNextEntry(new ZipEntry(name)); zip.write(text.getBytes(StandardCharsets.UTF_8)); zip.closeEntry();
    }
    private static void entry(ZipOutputStream zip, String name, InputStream input) throws Exception {
        zip.putNextEntry(new ZipEntry(name)); byte[] buffer = new byte[32768]; int count;
        while ((count = input.read(buffer)) != -1) zip.write(buffer, 0, count);
        zip.closeEntry();
    }
}
