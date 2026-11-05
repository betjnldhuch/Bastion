package online.bastion.launcher.core;

/** Ответ сервера на query-запрос. Неизменяемый, его можно передавать между потоками. */
public final class QueryResult {

    public enum State {
        /** Сервер ответил. */
        ONLINE,
        /** Ответа нет: сервер выключен, порт закрыт или нет сети. */
        OFFLINE,
        /** Домен не удалось превратить в IPv4-адрес. */
        UNRESOLVED
    }

    public final State state;
    public final boolean passworded;
    public final int players;
    public final int maxPlayers;
    public final String hostName;
    public final String gameMode;
    public final String language;
    /** Пинг в миллисекундах, -1 если неизвестен. */
    public final int pingMs;

    private QueryResult(State state, boolean passworded, int players, int maxPlayers,
                        String hostName, String gameMode, String language, int pingMs) {
        this.state = state;
        this.passworded = passworded;
        this.players = players;
        this.maxPlayers = maxPlayers;
        this.hostName = hostName;
        this.gameMode = gameMode;
        this.language = language;
        this.pingMs = pingMs;
    }

    public static QueryResult online(boolean passworded, int players, int maxPlayers,
                                     String hostName, String gameMode, String language, int pingMs) {
        return new QueryResult(State.ONLINE, passworded, players, maxPlayers, hostName, gameMode, language, pingMs);
    }

    public static QueryResult offline() {
        return new QueryResult(State.OFFLINE, false, 0, 0, "", "", "", -1);
    }

    public static QueryResult unresolved() {
        return new QueryResult(State.UNRESOLVED, false, 0, 0, "", "", "", -1);
    }

    public boolean isOnline() { return state == State.ONLINE; }
}
