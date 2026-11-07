// Bastion end-to-end check for the embedded SampDroid native client.
//
// Built on the host from the same SampDroid sources that go into the APK
// (after apply.py), then run in CI against a real open.mp server running the
// Bastion gamemode. It drives the session exactly like the Android UI does:
// connect -> PlayerInit -> (auto) RequestClass -> login/register dialogs ->
// server spawn -> PlayerSpawn ACK -> chat as a logged-in player.
//
// usage: bastion_e2e_client <host> <port> <nick> <password> <register|login>
#include "raknet_session.h"
#include "samp_query.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

void logLine(const char* stage, const std::string& text) {
  std::printf("[e2e] %-10s %s\n", stage, text.c_str());
  std::fflush(stdout);
}

int failWith(const std::string& why, const samp::SessionSnapshot& s) {
  std::printf("[e2e] FAIL: %s\n  error=%s\n  rpc=%s\n  state=%s\n", why.c_str(), s.error.c_str(),
              s.rpcSummary.c_str(), s.stateSummary.c_str());
  samp::raknet_session_disconnect();
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) {
    std::fprintf(stderr, "usage: %s host port nick password register|login\n", argv[0]);
    return 2;
  }
  const std::string host = argv[1];
  const uint16_t port = static_cast<uint16_t>(std::atoi(argv[2]));
  const std::string nick = argv[3];
  const std::string password = argv[4];
  const bool registering = std::string(argv[5]) == "register";

  // Same inputs the Android UI passes: the server's advertised query rules.
  std::string version, allowedClients;
  for (const samp::Rule& rule : samp::query_rules(host, port, 3000)) {
    if (rule.name == "version") version = rule.value;
    if (rule.name == "allowed_clients") allowedClients = rule.value;
  }
  logLine("query", "version=\"" + version + "\" allowed_clients=\"" + allowedClients + "\"");
  samp::SessionSnapshot s =
      samp::raknet_session_connect(host, port, 8000, nick, version, allowedClients);
  logLine("connect", std::string("active=") + (s.active ? "1" : "0") +
                         " accepted=" + (s.accepted ? "1" : "0") + " protocol=" + s.clientProtocol +
                         " transport=" + s.transportVariant + " err=" + s.error);
  if (!s.active || !s.accepted) return failWith("RakNet connection not accepted", s);

  bool sawInit = false, sawClass = false, answeredAuth = false, answeredGender = false;
  bool sawSpawnGrant = false, sentChat = false, sawWelcomeChat = false, sawRussianText = false;
  uint64_t answeredRevision = 0;
  size_t chatSeen = 0;
  const auto deadline = Clock::now() + std::chrono::seconds(45);
  while (Clock::now() < deadline) {
    s = samp::raknet_session_poll(100);
    if (!s.active) return failWith("session closed by server", s);
    const samp::WorldSnapshot w = samp::raknet_session_world_snapshot();

    if (!sawInit && s.playerInit) {
      sawInit = true;
      logLine("PlayerInit", "id=" + std::to_string(s.playerId) + " server=\"" + s.serverName +
                                "\" classes=" + std::to_string(s.spawnClassCount));
    }
    if (!sawClass && s.classSelectionKnown) {
      sawClass = true;
      logLine("class", std::string("RequestClass answered, selectable=") +
                           (s.classSelectable ? "1" : "0"));
    }
    for (; chatSeen < w.chat.size(); ++chatSeen) {
      const std::string& line = w.chat[chatSeen].text;
      logLine("chat", line);
      if (line.find('?') == std::string::npos && line.find("\xD0") != std::string::npos) {
        sawRussianText = true;
      }
      if (line.find("Bastion") != std::string::npos || line.find("\xD0\x94\xD0\xBE\xD0\xB1") != std::string::npos) {
        sawWelcomeChat = true;
      }
    }
    if (w.dialog.active && w.dialog.revision != answeredRevision) {
      answeredRevision = w.dialog.revision;
      logLine("dialog", "id=" + std::to_string(w.dialog.id) + " style=" +
                            std::to_string(w.dialog.style) + " title=\"" + w.dialog.title +
                            "\" buttons=\"" + w.dialog.button1 + "\"/\"" + w.dialog.button2 + "\"");
      if (w.dialog.title.find('?') == std::string::npos &&
          w.dialog.title.find("\xD0") != std::string::npos) {
        sawRussianText = true;
      }
      if (w.dialog.style == 3) {  // DIALOG_STYLE_PASSWORD: register or login
        s = samp::raknet_session_respond_dialog(w.dialog.id, w.dialog.revision, true, -1, password);
        answeredAuth = true;
        logLine("dialog", std::string("sent password (") + (registering ? "register" : "login") + ")");
      } else if (w.dialog.style == 2) {  // DIALOG_STYLE_LIST: gender after registration
        s = samp::raknet_session_respond_dialog(w.dialog.id, w.dialog.revision, true, 0, "");
        answeredGender = true;
        logLine("dialog", "chose list item 0");
      } else {
        s = samp::raknet_session_respond_dialog(w.dialog.id, w.dialog.revision, true, -1, "");
      }
      if (!s.error.empty()) logLine("dialog", "response error: " + s.error);
    }
    if (!sawSpawnGrant && s.spawnAllowed) {
      sawSpawnGrant = true;
      logLine("spawn", "server granted spawn, PlayerSpawn sent=" +
                           std::string(s.spawnConfirmationSent ? "1" : "0"));
    }
    if (s.spawnConfirmationAcked && !sentChat) {
      logLine("spawn", "PlayerSpawn ACKed; local pos=" + std::to_string(w.localX) + "," +
                           std::to_string(w.localY) + "," + std::to_string(w.localZ) +
                           " skin=" + std::to_string(w.localModel) +
                           " money=" + std::to_string(w.localMoney));
      s = samp::raknet_session_send_chat("/help");
      sentChat = true;
    }
    if (sentChat && s.spawnConfirmationAcked && answeredAuth && (!registering || answeredGender)) {
      // Let the server answer /help, then finish.
      const auto settle = Clock::now() + std::chrono::seconds(2);
      while (Clock::now() < settle) {
        s = samp::raknet_session_poll(100);
        const samp::WorldSnapshot w2 = samp::raknet_session_world_snapshot();
        for (; chatSeen < w2.chat.size(); ++chatSeen) logLine("chat", w2.chat[chatSeen].text);
      }
      if (!s.active) return failWith("server closed the session after spawn", s);
      logLine("result", std::string("OK: connected, ") + (registering ? "registered" : "logged in") +
                            ", class flow done, spawned. russianText=" + (sawRussianText ? "1" : "0") +
                            " welcomeChat=" + (sawWelcomeChat ? "1" : "0"));
      samp::raknet_session_disconnect();
      return sawRussianText ? 0 : 3;
    }
  }
  return failWith(std::string("timeout: init=") + (sawInit ? "1" : "0") + " class=" +
                      (sawClass ? "1" : "0") + " auth=" + (answeredAuth ? "1" : "0") +
                      " gender=" + (answeredGender ? "1" : "0") + " spawnGrant=" +
                      (sawSpawnGrant ? "1" : "0"),
                  s);
}
