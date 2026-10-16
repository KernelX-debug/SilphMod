package dev.rootcatch.ugphone;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Comparator;
import java.util.UUID;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

/** Private immutable directories. Export is serialized with farming; no automatic deletion. */
final class FarmReports {
    private FarmReports() { }
    static File create(File files) throws IOException {
        File report = new File(new File(files, "farm-reports"),
                System.currentTimeMillis() + "_" + UUID.randomUUID().toString().substring(0, 8));
        if (!report.mkdirs()) throw new IOException("Could not create report");
        return report;
    }
    static File[] recent(File files) {
        Map<String, File> combined = new LinkedHashMap<>();
        // Updating the installed app must not hide the previous UGPhone reports.
        for (String directory : new String[]{"farm-reports", "test-reports"}) {
            File root = new File(files, directory);
            if (!directChild(files, root)) continue;
            File[] entries = root.listFiles(file -> file.isDirectory()
                    && file.getName().matches("[0-9]{13}_[a-f0-9]{8}") && directChild(root, file));
            if (entries != null) for (File entry : entries) combined.putIfAbsent(entry.getName(), entry);
        }
        File[] reports = combined.values().toArray(new File[0]);
        Arrays.sort(reports, Comparator.comparing(File::getName).reversed());
        List<File> selected = new ArrayList<>(Arrays.asList(Arrays.copyOf(reports, Math.min(3, reports.length))));
        String chain = reports.length == 0 ? "" : chainId(reports[0]);
        if (!chain.isEmpty()) for (File report : reports) {
            if (selected.size() >= 12) break;
            if (chain.equals(chainId(report)) && !selected.contains(report)) selected.add(report);
        }
        selected.sort(Comparator.comparing(File::getName).reversed());
        return selected.toArray(new File[0]);
    }
    static void chain(File report, String id) throws IOException {
        if (id == null || !id.matches("[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}"))
            throw new IllegalArgumentException("Invalid startup chain");
        Files.write(new File(report, "startup-chain.txt").toPath(), id.getBytes(StandardCharsets.UTF_8));
    }
    private static String chainId(File report) {
        File file = new File(report, "startup-chain.txt");
        if (!directChild(report, file) || !file.isFile() || file.length() != 36) return "";
        try {
            String value = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
            return value.matches("[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}") ? value : "";
        } catch (IOException error) { return ""; }
    }
    static boolean allowedFile(String name) {
        return name.equals("report-status.json") || name.equals("engine-start.jsonl") || name.equals("startup-chain.txt")
                || name.matches("(?:farm-session|test-session|latest|health|sources)(?:-[12])?\\.jsonl")
                || name.matches("logcat-live(?:-[12])?\\.log")
                || name.matches("tombstone-inventory-(?:before|after)\\.log")
                || name.matches("(?:before|after)-(?:logcat|game-process|process-list|activity|window|kernel|dropbox-native|dropbox-java|tools)\\.log")
                || name.matches("(?:historical|current)-tombstone_[0-9]{2,4}\\.log");
    }
    static File[] files(File report) {
        File[] entries = report.listFiles(file -> file.isFile() && allowedFile(file.getName()) && directChild(report, file));
        if (entries == null) return new File[0];
        Arrays.sort(entries, Comparator.comparing(File::getName));
        return entries;
    }
    private static boolean directChild(File parent, File file) {
        try { return file.getCanonicalFile().equals(new File(parent.getCanonicalFile(), file.getName())); }
        catch (IOException error) { return false; }
    }
}
