package online.bastion.launcher.net;

import android.os.Handler;
import android.os.Looper;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

import online.bastion.launcher.core.QueryResult;
import online.bastion.launcher.core.SampQuery;
import online.bastion.launcher.core.ServerAddress;
import online.bastion.launcher.core.ServerEntry;

/**
 * Периодически опрашивает сервера в фоне и сообщает результат в главный поток.
 *
 * - Запросы идут в отдельных потоках, UI никогда не ждёт сеть.
 * - Пока предыдущий опрос сервера не закончился, новый к нему не отправляется.
 * - Если адрес сервера поменяли во время опроса, устаревший ответ отбрасывается.
 */
public final class ServerMonitor {

    public interface Listener {
        /** Вызывается в главном потоке. */
        void onStatus(ServerEntry server, Status status);
    }

    /** Состояние сервера для отображения. */
    public static final class Status {
        public final ServerAddress address;
        /** null — опрос ещё идёт и предыдущего результата нет. */
        public final QueryResult result;
        public final boolean checking;

        Status(ServerAddress address, QueryResult result, boolean checking) {
            this.address = address;
            this.result = result;
            this.checking = checking;
        }
    }

    public interface AddressSource {
        ServerAddress addressOf(ServerEntry server);
    }

    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService pool = Executors.newFixedThreadPool(2, new ThreadFactory() {
        private int n;
        @Override public synchronized Thread newThread(Runnable r) {
            Thread t = new Thread(r, "server-query-" + (++n));
            t.setDaemon(true);
            return t;
        }
    });

    private final Iterable<ServerEntry> servers;
    private final AddressSource addresses;
    private final long intervalMs;
    private final Map<String, Status> last = new HashMap<String, Status>();
    private final Set<String> inFlight = new HashSet<String>();
    private Listener listener;
    private boolean running;

    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            refreshAll();
            main.postDelayed(this, intervalMs);
        }
    };

    public ServerMonitor(Iterable<ServerEntry> servers, AddressSource addresses, int intervalSec) {
        this.servers = servers;
        this.addresses = addresses;
        this.intervalMs = intervalSec * 1000L;
    }

    public void setListener(Listener l) { listener = l; }

    /** Последний известный статус (или null). Только из главного потока. */
    public Status statusOf(ServerEntry server) { return last.get(server.id); }

    /** Запустить периодический опрос (onResume). */
    public void start() {
        if (running) return;
        running = true;
        main.post(tick);
    }

    /** Остановить опрос (onPause). Уже отправленные запросы доработают и обновят статус. */
    public void stop() {
        running = false;
        main.removeCallbacks(tick);
    }

    /** Окончательно освободить потоки (onDestroy). */
    public void shutdown() {
        stop();
        listener = null;
        pool.shutdownNow();
    }

    public void refreshAll() {
        for (ServerEntry s : servers) refresh(s);
    }

    /** Опросить один сервер сейчас. Только из главного потока. */
    public void refresh(final ServerEntry server) {
        if (pool.isShutdown() || inFlight.contains(server.id)) return;
        final ServerAddress address = addresses.addressOf(server);
        Status previous = last.get(server.id);
        // Если адрес сменился, старый онлайн/пинг относится к другому серверу — сбрасываем.
        QueryResult keep = previous != null && address.equals(previous.address) ? previous.result : null;
        publish(server, new Status(address, keep, true));
        inFlight.add(server.id);

        pool.execute(new Runnable() {
            @Override public void run() {
                final QueryResult result = SampQuery.query(address);
                main.post(new Runnable() {
                    @Override public void run() {
                        inFlight.remove(server.id);
                        if (!address.equals(addresses.addressOf(server))) {
                            refresh(server);  // адрес поменяли, пока ждали ответа
                            return;
                        }
                        publish(server, new Status(address, result, false));
                    }
                });
            }
        });
    }

    private void publish(ServerEntry server, Status status) {
        last.put(server.id, status);
        if (listener != null) listener.onStatus(server, status);
    }
}
