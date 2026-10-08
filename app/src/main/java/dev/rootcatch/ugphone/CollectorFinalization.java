package dev.rootcatch.ugphone;

/** Every shutdown step runs, even when gathering sources or a previous shutdown step fails. */
final class CollectorFinalization {
    private CollectorFinalization() { }
    static void run(Runnable gather, Runnable stopLive, Runnable recordEnd, Runnable closeManifest) {
        try { gather.run(); }
        finally {
            try { stopLive.run(); }
            finally {
                try { recordEnd.run(); }
                finally { closeManifest.run(); }
            }
        }
    }
}
