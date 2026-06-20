#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#if __has_include("local_secrets.h")
#include "local_secrets.h"
#else
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* otaHostname = "esp32-relay-x8";
const char* otaPassword = "YOUR_OTA_PASSWORD";
#endif

// Relay mapping from eMariete ESP32 Relay Board x8 article.
const uint8_t RELAY_COUNT = 8;
const uint8_t relayPins[RELAY_COUNT] = {32, 33, 25, 26, 27, 14, 12, 13};

// According to the board docs, this board uses active HIGH relays.
const bool RELAY_ACTIVE_HIGH = true;
const char* relayDefaultNames[RELAY_COUNT] = {
  "Relay 1", "Relay 2", "Relay 3", "Relay 4",
  "Relay 5", "Relay 6", "Relay 7", "Relay 8"
};

WebServer server(80);
Preferences preferences;
bool relayState[RELAY_COUNT] = {false, false, false, false, false, false, false, false};
String relayNames[RELAY_COUNT];

String htmlPage() {
  String html = R"rawliteral(
<!doctype html>
<html lang="pt">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 Relay x8</title>
  <style>
    :root {
      --bg: #0f172a;
      --panel: #111827;
      --card: #1f2937;
      --text: #e5e7eb;
      --muted: #9ca3af;
      --on: #16a34a;
      --off: #ef4444;
      --accent: #0ea5e9;
    }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, sans-serif;
      color: var(--text);
      background: radial-gradient(circle at 20% 20%, #1e293b 0%, var(--bg) 45%, #020617 100%);
      display: grid;
      place-items: center;
      padding: 18px;
      box-sizing: border-box;
    }
    .wrap {
      width: min(900px, 100%);
      background: linear-gradient(180deg, rgba(255,255,255,0.06), rgba(255,255,255,0.02));
      border: 1px solid rgba(255,255,255,0.12);
      border-radius: 16px;
      padding: 18px;
      box-shadow: 0 10px 35px rgba(0,0,0,0.35);
      backdrop-filter: blur(4px);
    }
    h1 {
      margin: 0 0 6px;
      font-size: clamp(1.3rem, 2.5vw, 2rem);
      letter-spacing: 0.3px;
    }
    p {
      margin: 0 0 16px;
      color: var(--muted);
      font-size: 0.95rem;
    }
    .actions {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-bottom: 16px;
    }
    button {
      border: 0;
      border-radius: 10px;
      padding: 10px 14px;
      color: #fff;
      font-weight: 700;
      cursor: pointer;
      transition: transform 0.08s ease, opacity 0.2s ease;
    }
    button:active {
      transform: translateY(1px);
    }
    .all-on { background: var(--on); }
    .all-off { background: var(--off); }
    .refresh { background: var(--accent); }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 12px;
    }
    .card {
      background: var(--card);
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 12px;
      padding: 12px;
      display: grid;
      gap: 10px;
    }
    .tag {
      font-weight: 800;
      font-size: 1rem;
    }
    .state {
      padding: 6px 10px;
      border-radius: 999px;
      width: fit-content;
      font-weight: 700;
      font-size: 0.85rem;
    }
    .state.on { background: rgba(22,163,74,0.2); color: #86efac; border: 1px solid rgba(22,163,74,0.45); }
    .state.off { background: rgba(239,68,68,0.2); color: #fca5a5; border: 1px solid rgba(239,68,68,0.45); }
    .row {
      display: flex;
      gap: 8px;
    }
    .name-row {
      display: grid;
      grid-template-columns: minmax(0, 1fr);
      gap: 8px;
    }
    .name-input {
      min-width: 0;
      border: 1px solid rgba(255,255,255,0.12);
      border-radius: 8px;
      padding: 9px 10px;
      background: rgba(255,255,255,0.06);
      color: var(--text);
      outline: none;
    }
    .name-input::placeholder {
      color: #94a3b8;
    }
    .save-btn {
      background: #0369a1;
      width: 100%;
    }
    .on-btn, .off-btn {
      flex: 1;
      min-width: 0;
      padding: 10px 8px;
    }
    .on-btn { background: #15803d; }
    .off-btn { background: #b91c1c; }
    .meta {
      margin-top: 14px;
      color: var(--muted);
      font-size: 0.82rem;
      line-height: 1.4;
      word-break: break-word;
    }
    .saved-note {
      color: #7dd3fc;
      font-size: 0.8rem;
      min-height: 1em;
    }
  </style>
</head>
<body>
  <main class="wrap">
    <h1>ESP32 Relay Board x8</h1>
    <p>Controlo individual dos 8 relays + API HTTP para Node-RED.</p>

    <div class="actions">
      <button class="all-on" onclick="setAll(1)">Ligar Todos</button>
      <button class="all-off" onclick="setAll(0)">Desligar Todos</button>
      <button class="refresh" onclick="refreshStatus()">Atualizar</button>
    </div>

    <section class="grid" id="grid"></section>

    <div class="meta" id="meta"></div>
  </main>

<script>
  const relayCount = 8;
  const grid = document.getElementById('grid');
  const meta = document.getElementById('meta');

  function relayCard(index) {
    return `
      <article class="card" id="card-${index}">
        <div class="tag" id="name-label-${index}">Relay ${index}</div>
        <div class="state off" id="state-${index}">OFF</div>
        <div class="name-row">
          <input class="name-input" id="name-input-${index}" maxlength="32" placeholder="Nome do relay" />
          <button class="save-btn" onclick="saveName(${index})">Guardar</button>
        </div>
        <div class="saved-note" id="saved-note-${index}"></div>
        <div class="row">
          <button class="on-btn" onclick="setRelay(${index}, 1)">ON</button>
          <button class="off-btn" onclick="setRelay(${index}, 0)">OFF</button>
        </div>
      </article>
    `;
  }

  for (let i = 1; i <= relayCount; i++) {
    grid.insertAdjacentHTML('beforeend', relayCard(i));
  }

  async function api(path) {
    const r = await fetch(path);
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    return r.json();
  }

  function paintStatus(data) {
    const states = data.relays || [];
    const names = data.names || [];
    for (let i = 0; i < relayCount; i++) {
      const el = document.getElementById(`state-${i + 1}`);
      const label = document.getElementById(`name-label-${i + 1}`);
      const input = document.getElementById(`name-input-${i + 1}`);
      const note = document.getElementById(`saved-note-${i + 1}`);
      const on = !!states[i];
      const name = names[i] || `Relay ${i + 1}`;
      el.textContent = on ? 'ON' : 'OFF';
      el.className = on ? 'state on' : 'state off';
      label.textContent = name;
      if (document.activeElement !== input) {
        input.value = name;
      }
      if (note && !note.dataset.locked) {
        note.textContent = '';
      }
    }
    meta.textContent = `IP: ${location.host} | Uptime(s): ${data.uptime || 0}`;
  }

  async function refreshStatus() {
    try {
      const data = await api('/api/status');
      paintStatus(data);
    } catch (e) {
      meta.textContent = 'Erro a ler estado: ' + e.message;
    }
  }

  async function setRelay(ch, state) {
    try {
      const data = await api(`/api/relay?ch=${ch}&state=${state}`);
      paintStatus(data);
    } catch (e) {
      meta.textContent = 'Erro ao enviar comando: ' + e.message;
    }
  }

  async function setAll(state) {
    try {
      const data = await api(`/api/all?state=${state}`);
      paintStatus(data);
    } catch (e) {
      meta.textContent = 'Erro ao enviar comando global: ' + e.message;
    }
  }

  async function saveName(ch) {
    try {
      const input = document.getElementById(`name-input-${ch}`);
      const note = document.getElementById(`saved-note-${ch}`);
      const name = encodeURIComponent(input.value.trim());
      const data = await api(`/api/name?ch=${ch}&name=${name}`);
      paintStatus(data);
      if (note) {
        note.textContent = 'Nome guardado';
        note.dataset.locked = '1';
        setTimeout(() => {
          note.textContent = '';
          delete note.dataset.locked;
        }, 2500);
      }
      meta.textContent = `Nome do relay ${ch} guardado.`;
    } catch (e) {
      meta.textContent = 'Erro ao guardar nome: ' + e.message;
    }
  }

  refreshStatus();
  setInterval(refreshStatus, 5000);
</script>
</body>
</html>
)rawliteral";
  return html;
}

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void writeRelay(uint8_t idx, bool on) {
  if (idx >= RELAY_COUNT) return;

  relayState[idx] = on;
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(relayPins[idx], on ? HIGH : LOW);
  } else {
    digitalWrite(relayPins[idx], on ? LOW : HIGH);
  }
}

void writeAll(bool on) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    writeRelay(i, on);
  }
}

String sanitizeRelayName(const String& value, uint8_t idx) {
  String sanitized = value;
  sanitized.trim();
  sanitized.replace("\r", " ");
  sanitized.replace("\n", " ");
  if (sanitized.length() == 0) {
    sanitized = relayDefaultNames[idx];
  }
  if (sanitized.length() > 32) {
    sanitized = sanitized.substring(0, 32);
  }
  return sanitized;
}

void loadRelayNames() {
  preferences.begin("relay-ui", true);
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    String key = "name" + String(i);
    relayNames[i] = sanitizeRelayName(preferences.getString(key.c_str(), relayDefaultNames[i]), i);
  }
  preferences.end();
}

bool saveRelayName(uint8_t idx, const String& newName) {
  if (idx >= RELAY_COUNT) return false;

  relayNames[idx] = sanitizeRelayName(newName, idx);
  preferences.begin("relay-ui", false);
  String key = "name" + String(idx);
  bool ok = preferences.putString(key.c_str(), relayNames[idx]) > 0;
  preferences.end();
  return ok;
}

String jsonStatus() {
  String out = "{\"ok\":true,\"relays\":[";
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    out += (relayState[i] ? "1" : "0");
    if (i < RELAY_COUNT - 1) out += ",";
  }
  out += "],\"names\":[";
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    String escaped = relayNames[i];
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    out += "\"";
    out += escaped;
    out += "\"";
    if (i < RELAY_COUNT - 1) out += ",";
  }
  out += "],\"uptime\":";
  out += String(millis() / 1000UL);
  out += "}";
  return out;
}

void sendJson(int code, const String& payload) {
  addCorsHeaders();
  server.send(code, "application/json", payload);
}

void handleRoot() {
  addCorsHeaders();
  server.send(200, "text/html; charset=utf-8", htmlPage());
}

void handleStatus() {
  sendJson(200, jsonStatus());
}

bool parseOnOffValue(const String& value, bool& outOn) {
  if (value == "1" || value == "on" || value == "ON" || value == "true") {
    outOn = true;
    return true;
  }
  if (value == "0" || value == "off" || value == "OFF" || value == "false") {
    outOn = false;
    return true;
  }
  return false;
}

void handleRelayQuery() {
  if (!server.hasArg("ch") || !server.hasArg("state")) {
    sendJson(400, "{\"ok\":false,\"error\":\"Missing ch or state\"}");
    return;
  }

  int ch = server.arg("ch").toInt();
  if (ch < 1 || ch > RELAY_COUNT) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid ch (1..8)\"}");
    return;
  }

  bool on = false;
  if (!parseOnOffValue(server.arg("state"), on)) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid state (0/1/on/off)\"}");
    return;
  }

  writeRelay(ch - 1, on);
  sendJson(200, jsonStatus());
}

void handleAllQuery() {
  if (!server.hasArg("state")) {
    sendJson(400, "{\"ok\":false,\"error\":\"Missing state\"}");
    return;
  }

  bool on = false;
  if (!parseOnOffValue(server.arg("state"), on)) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid state (0/1/on/off)\"}");
    return;
  }

  writeAll(on);
  sendJson(200, jsonStatus());
}

void handleNameQuery() {
  if (!server.hasArg("ch") || !server.hasArg("name")) {
    sendJson(400, "{\"ok\":false,\"error\":\"Missing ch or name\"}");
    return;
  }

  int ch = server.arg("ch").toInt();
  if (ch < 1 || ch > RELAY_COUNT) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid ch (1..8)\"}");
    return;
  }

  if (!saveRelayName(ch - 1, server.arg("name"))) {
    sendJson(500, "{\"ok\":false,\"error\":\"Failed to save name\"}");
    return;
  }

  sendJson(200, jsonStatus());
}

void handleOptions() {
  addCorsHeaders();
  server.send(204);
}

void handleNotFound() {
  const String uri = server.uri();

  // Extra path-based API for simple Node-RED calls:
  // /api/relay/1/on, /api/relay/1/off, /api/all/on, /api/all/off
  if (uri.startsWith("/api/relay/")) {
    int p1 = uri.indexOf('/', 11);
    if (p1 > 0) {
      String chStr = uri.substring(11, p1);
      String stateStr = uri.substring(p1 + 1);
      int ch = chStr.toInt();
      bool on = false;

      if (ch >= 1 && ch <= RELAY_COUNT && parseOnOffValue(stateStr, on)) {
        writeRelay(ch - 1, on);
        sendJson(200, jsonStatus());
        return;
      }
    }
  }

  if (uri.startsWith("/api/all/")) {
    String stateStr = uri.substring(9);
    bool on = false;
    if (parseOnOffValue(stateStr, on)) {
      writeAll(on);
      sendJson(200, jsonStatus());
      return;
    }
  }

  sendJson(404, "{\"ok\":false,\"error\":\"Not found\"}");
}

void setupRelays() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
  }

  // Safe startup: all relays OFF.
  writeAll(false);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("A ligar ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("WiFi ligado. IP: ");
  Serial.println(WiFi.localIP());
}

void setupOta() {
  ArduinoOTA.setHostname(otaHostname);
  ArduinoOTA.setPassword(otaPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA end");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error %u\n", error);
  });

  ArduinoOTA.begin();
  Serial.println("OTA pronto");
}

void setupHttpServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay", HTTP_GET, handleRelayQuery);
  server.on("/api/all", HTTP_GET, handleAllQuery);
  server.on("/api/name", HTTP_GET, handleNameQuery);
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/relay", HTTP_OPTIONS, handleOptions);
  server.on("/api/all", HTTP_OPTIONS, handleOptions);
  server.on("/api/name", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server pronto");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  loadRelayNames();
  setupRelays();
  setupWiFi();
  setupOta();
  setupHttpServer();

  Serial.println("Sistema pronto");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
