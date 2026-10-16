package dev.rootcatch.nativeengine;

import java.util.EnumSet;
import java.util.Set;

public final class ConnectionPlan {
    private final ConnectionCapabilities capabilities;
    private final Set<ConnectionStrategy> tried = EnumSet.noneOf(ConnectionStrategy.class);
    private ConnectionStrategy selected;
    private boolean cached;
    public ConnectionPlan(ConnectionCapabilities capabilities, String cachedRoute) {
        this.capabilities = capabilities;
        capabilities.preferred();
        try {
            ConnectionStrategy previous = ConnectionStrategy.valueOf(cachedRoute == null ? "" : cachedRoute);
            if (capabilities.allows(previous)) { selected = previous; cached = true; }
        } catch (IllegalArgumentException ignored) { }
        if (selected == null) selected = capabilities.preferred();
        tried.add(selected);
    }
    public ConnectionStrategy selected() { return selected; }
    public boolean cached() { return cached; }
    public ConnectionStrategy next(ConnectionFailure failure) {
        if (failure == null || !failure.retryable()) return null;
        ConnectionStrategy next = cached && capabilities.preferred() != selected ? capabilities.preferred() : failure.alternative(selected);
        while (next != null && next != ConnectionStrategy.COMPAT_ATTACH && tried.contains(next)) next = failure.alternative(next);
        return capabilities.allows(next) ? next : null;
    }
    public void advance(ConnectionStrategy next) {
        if (next == null || !capabilities.allows(next)) throw new IllegalArgumentException("Invalid connection alternative");
        selected = next; tried.add(next); cached = false;
    }
}
