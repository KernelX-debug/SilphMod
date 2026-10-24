package dev.rootcatch.nativeengine;

import dev.rootcatch.root.RootShell;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

public final class NativeFocus {
    private final FocusReader reader;
    private final FocusLogPolicy policy=new FocusLogPolicy();
    public NativeFocus(RootShell shell,BooleanSupplier stopping,Consumer<String> log) {
        reader=new FocusReader((command,timeout)->{RootShell.Result r=shell.execute(command,timeout);return new FocusReader.Reply(r.code,r.output);},stopping,
                sample->{if(policy.record(sample,android.os.SystemClock.elapsedRealtime(),true))log.accept("focus source="+sample.source+" state="+sample.focus.state+" reason="+sample.focus.reason+" ms="+sample.durationMs+"\n"+sample.response);});
    }
    public boolean active(){return reader.read("native_check").focus.isGame();}
}
