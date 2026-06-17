// Doorbell_C3_Webhook
// --------------------------------------------------------------------------
// Intercom-ring doorbell notifier for ESP32-C3 (e.g. "C3 Super Mini / WiFi Mini").
//
// Power / signal model (IMPORTANT):
//   - VCC comes from the building's CENTRAL intercom bus. It energizes whenever
//     ANYONE's bell in the building is pressed, and then stays up for a few
//     minutes. Power-up is therefore NOT a signal that OUR apartment was rung.
//   - The actual ring for THIS apartment is the RING SIGNAL on SIGNAL_PIN
//     changing state. We notify ONLY on a ring-signal transition, never on boot.
//   - During the multi-minute power window the bell may be pressed several times;
//     each press must produce its own notification.
//
//   power-up (maybe a neighbor) -> connect WiFi, wait
//   our ring line asserts       -> notify  (repeat per press)
//   power drops (~minutes later) -> board dies
//
// Ring detection: an analog intercom ring is a BUZZING BURST -> the line toggles
// many times per press. A 3-state machine collapses each burst into ONE ring:
//   IDLE -> (line active for >= RING_DEBOUNCE_MS, rejects glitches) -> ACTIVE (notify)
//   ACTIVE -> (line idle for >= RING_RELEASE_MS) -> IDLE (armed for next press)
// Brief idle gaps inside a buzz (< RING_RELEASE_MS) do NOT end the ring.
//
// Speed on connect: static IP (skips DHCP) + cached BSSID/channel in NVS (skips
// the WiFi scan; survives power loss) so the first real ring notifies fast.
//
// Wiring: VCC, GND, and the interceptor ring signal -> SIGNAL_PIN.
// Flashing: no OTA (powered only when the bus is live). Reflash on the bench.
// --------------------------------------------------------------------------

#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASS, HA_WEBHOOK_URL — gitignored; see secrets.example.h

// Address via DHCP (device lives on the IoT VLAN). Pin it with a router
// reservation if you want a stable IP. The fast-connect win comes from the
// cached BSSID/channel below (no scan), not from skipping DHCP.

// ---- Ring inputs (a ring on EITHER pin notifies) ----
//   RING_PIN   (GPIO3) : interceptor ring line — the real doorbell.
//   BUTTON_PIN (GPIO9) : on-board BOOT button — a permanent manual test trigger,
//                        handy once the board is installed out of reach.
// Both are active-low (idle pulled HIGH; pressed/ringing pulls to GND).
// Don't HOLD BOOT during a reset — that enters the bootloader.
#define RING_PIN       3
#define BUTTON_PIN     9
#define RING_ACTIVE    LOW

#define CONNECT_TIMEOUT_MS   8000
#define FASTPATH_GIVEUP_MS   2500   // if cached BSSID hasn't connected by now, rescan
#define RING_DEBOUNCE_MS     40     // line must be active this long to count (glitch reject)
#define RING_RELEASE_MS      1500   // line idle this long ends the ring (arms next press)
#define POST_RETRIES         3
#define POST_TIMEOUT_MS      2000

Preferences prefs;

enum RingState { IDLE, CANDIDATE, ACTIVE };
RingState ringState   = IDLE;
uint32_t  candidateMs = 0;       // when the current candidate burst started
uint32_t  lastActiveMs = 0;      // last time the line read active

static bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.persistent(false);              // we manage our own cache via NVS
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // DHCP — IoT VLAN assigns the address

  // ---- fast path: reuse last known BSSID + channel (skips the scan) ----
  prefs.begin("wifi", true);
  uint8_t  bssid[6];
  size_t   got = prefs.getBytes("bssid", bssid, sizeof(bssid));
  int32_t  channel = prefs.getInt("chan", 0);
  prefs.end();

  bool fastPath = (got == sizeof(bssid) && channel > 0);
  if (fastPath) WiFi.begin(WIFI_SSID, WIFI_PASS, channel, bssid, true);
  else          WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  bool rescanned = false;
  while (WiFi.status() != WL_CONNECTED) {
    if (fastPath && !rescanned && (millis() - start) > FASTPATH_GIVEUP_MS) {
      WiFi.disconnect();                 // cached AP didn't answer -> full scan
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      rescanned = true;
    }
    if ((millis() - start) > CONNECT_TIMEOUT_MS) return false;
    delay(10);
  }

  uint8_t* b = WiFi.BSSID();             // cache the AP we landed on for next ring
  if (b) {
    prefs.begin("wifi", false);
    prefs.putBytes("bssid", b, 6);
    prefs.putInt("chan", WiFi.channel());
    prefs.end();
  }
  return true;
}

static bool notifyHA() {
  if (!connectWiFi()) { Serial.println("WiFi down, cannot notify"); return false; }

  char body[120];
  snprintf(body, sizeof(body), "{\"event\":\"pressed\",\"rssi\":%d}", WiFi.RSSI());

  for (int attempt = 1; attempt <= POST_RETRIES; attempt++) {
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(POST_TIMEOUT_MS);
    http.setTimeout(POST_TIMEOUT_MS);
    if (http.begin(client, HA_WEBHOOK_URL)) {
      http.addHeader("Content-Type", "application/json");
      int code = http.POST((uint8_t*)body, strlen(body));
      http.end();
      Serial.printf("ring POST attempt %d -> %d\n", attempt, code);
      if (code >= 200 && code < 300) return true;
    }
    delay(150);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(RING_PIN,   INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  bool ok = connectWiFi();       // get the link ready; do NOT notify on boot
  Serial.printf("WiFi %s  IP=%s  RSSI=%d\n",
                ok ? "OK" : "FAIL", WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void loop() {
  uint32_t now = millis();
  bool active = (digitalRead(RING_PIN)   == RING_ACTIVE) ||   // real doorbell
                (digitalRead(BUTTON_PIN) == RING_ACTIVE);     // BOOT = manual test

  switch (ringState) {
    case IDLE:
      if (active) { ringState = CANDIDATE; candidateMs = now; lastActiveMs = now; }
      break;

    case CANDIDATE:                                   // confirm it's a real ring, not a glitch
      if (active) {
        lastActiveMs = now;
        if (now - candidateMs >= RING_DEBOUNCE_MS) { ringState = ACTIVE; notifyHA(); }
      } else if (now - lastActiveMs > RING_RELEASE_MS) {
        ringState = IDLE;                             // never sustained -> glitch, drop it
      }
      break;

    case ACTIVE:                                      // inside a confirmed ring (buzz burst)
      if (active) lastActiveMs = now;
      else if (now - lastActiveMs > RING_RELEASE_MS) ringState = IDLE;   // released -> arm next
      break;
  }

  if (WiFi.status() != WL_CONNECTED) connectWiFi();   // keep link healthy across the window
  delay(5);
}
