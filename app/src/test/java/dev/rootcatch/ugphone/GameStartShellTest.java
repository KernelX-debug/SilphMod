package dev.rootcatch.ugphone;

import dev.rootcatch.testutil.TestShell;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;
import org.junit.Assume;
import org.junit.Test;
import static org.junit.Assert.*;

/** Syntax and harmless framing fixtures only; never runs proc readers or force-stop on the computer. */
public final class GameStartShellTest {
    @Test public void allGeneratedCommandsHaveValidShellSyntax() throws Exception {
        String bash = TestShell.bash();
        for (String command : new String[]{GameStartCheck.framed("PIDOF", GameStartCheck.PIDOF),
                GameStartCheck.framed("PROC", GameStartCheck.PROC), GameStartCheck.framed("STOP_GAME", GameStartCheck.STOP_GAME),
                GameStartCheck.framed("LIMIT_STOP", SessionLimitStop.command(1234, 56789)),
                GameStartCheck.framed("TARGET", SessionLimitStop.targetCommand(1234))}) {
            Process process = new ProcessBuilder(bash, "--noprofile", "--norc", "-n").redirectErrorStream(true).start();
            try {
                process.getOutputStream().write(command.getBytes(StandardCharsets.UTF_8)); process.getOutputStream().close();
                assertTrue(process.waitFor(5, TimeUnit.SECONDS));
                assertEquals(new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8), 0, process.exitValue());
            } finally { process.destroyForcibly(); }
        }
    }
    @Test public void framePreservesMergedOutputExitStatusAndSubsequentShellCommand() throws Exception {
        String bash = TestShell.bash();
        String command = GameStartCheck.framed("PIDOF", "printf 'out\\n\\n'; printf 'err\\n' >&2; (exit 7)");
        Process process = new ProcessBuilder(bash, "--noprofile", "--norc").redirectErrorStream(true).start();
        try {
            process.getOutputStream().write((command + "; rc_fixture_exit=$?; printf '\\nFOLLOWUP:%s\\n' \"$rc_fixture_exit\"\n").getBytes(StandardCharsets.UTF_8));
            process.getOutputStream().close(); assertTrue(process.waitFor(5, TimeUnit.SECONDS));
            String output = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
            assertTrue(output, output.endsWith("\nFOLLOWUP:7\n"));
            String frame = output.substring(0, output.indexOf("\nFOLLOWUP:")).trim();
            assertEquals("out\n\nerr\n", GameStartCheck.decode("PIDOF", new GameStartCheck.Reply(7, frame)).payload);
        } finally { process.destroyForcibly(); }
    }
}
