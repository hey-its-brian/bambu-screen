#include "bambu.h"
#include "config.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

PrinterState printerState;

// Bambu LAN-mode reports are large JSON blobs (several KB); PubSubClient
// silently drops any message bigger than its buffer.
static const uint16_t MQTT_BUFFER_SIZE = 16384;
static const uint32_t RECONNECT_INTERVAL_MS = 5000;

static WiFiClientSecure tlsClient;
static PubSubClient mqtt(tlsClient);

static char topicReport[64];
static char topicRequest[64];
static uint32_t lastConnectAttempt = 0;
static uint32_t sequenceId = 100;

static void publishJson(JsonDocument& doc) {
  if (!mqtt.connected()) {
    Serial.println("[bambu] publish skipped: not connected");
    return;
  }
  char buf[512];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  bool ok = mqtt.publish(topicRequest, (const uint8_t*)buf, n);
  Serial.printf("[bambu] publish %s: %s\n", ok ? "ok" : "FAILED", buf);
}

static void requestPushAll() {
  StaticJsonDocument<128> doc;
  JsonObject pushing = doc.createNestedObject("pushing");
  pushing["sequence_id"] = String(sequenceId++);
  pushing["command"] = "pushall";
  publishJson(doc);
}

static void sendPrintCommand(const char* command, const char* param = "") {
  StaticJsonDocument<192> doc;
  JsonObject print = doc.createNestedObject("print");
  print["sequence_id"] = String(sequenceId++);
  print["command"] = command;
  print["param"] = param;
  publishJson(doc);
}

void bambuPause()  { sendPrintCommand("pause"); }
void bambuResume() { sendPrintCommand("resume"); }
void bambuStop()   { sendPrintCommand("stop"); }

void bambuSetSpeed(int level) {
  if (level < 1) level = 1;
  if (level > 4) level = 4;
  char param[2] = { (char)('0' + level), 0 };
  sendPrintCommand("print_speed", param);
}

void bambuSetLight(bool on) {
  StaticJsonDocument<256> doc;
  JsonObject system = doc.createNestedObject("system");
  system["sequence_id"] = String(sequenceId++);
  system["command"] = "ledctrl";
  system["led_node"] = "chamber_light";
  system["led_mode"] = on ? "on" : "off";
  system["led_on_time"] = 500;
  system["led_off_time"] = 500;
  system["loop_times"] = 0;
  system["interval_time"] = 0;
  publishJson(doc);
}

const char* bambuSpeedName(int level) {
  switch (level) {
    case 1: return "Silent";
    case 2: return "Standard";
    case 3: return "Sport";
    case 4: return "Ludicrous";
    default: return "?";
  }
}

static void copyStr(char* dst, size_t dstSize, const char* src) {
  if (!src) return;
  strlcpy(dst, src, dstSize);
}

static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  // Filter keeps the parsed document small; the raw report has hundreds of keys.
  StaticJsonDocument<768> filter;
  JsonObject fp = filter.createNestedObject("print");
  fp["gcode_state"] = true;
  fp["subtask_name"] = true;
  fp["mc_percent"] = true;
  fp["mc_remaining_time"] = true;
  fp["layer_num"] = true;
  fp["total_layer_num"] = true;
  fp["nozzle_temper"] = true;
  fp["nozzle_target_temper"] = true;
  fp["bed_temper"] = true;
  fp["bed_target_temper"] = true;
  fp["spd_lvl"] = true;
  fp["lights_report"] = true;

  DynamicJsonDocument doc(4096);
  DeserializationError err =
      deserializeJson(doc, payload, length, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[bambu] JSON parse failed: %s\n", err.c_str());
    return;
  }

  JsonObject print = doc["print"];
  if (print.isNull()) return;

  PrinterState& s = printerState;
  // Reports can be partial; only update fields that are present.
  if (print.containsKey("gcode_state"))      copyStr(s.gcodeState, sizeof(s.gcodeState), print["gcode_state"]);
  if (print.containsKey("subtask_name"))     copyStr(s.taskName, sizeof(s.taskName), print["subtask_name"]);
  if (print.containsKey("mc_percent"))       s.percent = print["mc_percent"].as<int>();
  if (print.containsKey("mc_remaining_time")) s.remainingMin = print["mc_remaining_time"].as<int>();
  if (print.containsKey("layer_num"))        s.layer = print["layer_num"].as<int>();
  if (print.containsKey("total_layer_num"))  s.totalLayers = print["total_layer_num"].as<int>();
  if (print.containsKey("nozzle_temper"))    s.nozzleTemp = print["nozzle_temper"].as<float>();
  if (print.containsKey("nozzle_target_temper")) s.nozzleTarget = print["nozzle_target_temper"].as<float>();
  if (print.containsKey("bed_temper"))       s.bedTemp = print["bed_temper"].as<float>();
  if (print.containsKey("bed_target_temper")) s.bedTarget = print["bed_target_temper"].as<float>();
  if (print.containsKey("spd_lvl"))          s.speedLevel = print["spd_lvl"].as<int>();

  JsonArray lights = print["lights_report"];
  for (JsonObject light : lights) {
    Serial.printf("[bambu] light node=%s mode=%s\n",
                  light["node"] | "?", light["mode"] | "?");
    if (strcmp(light["node"] | "", "chamber_light") == 0) {
      s.chamberLight = strcmp(light["mode"] | "off", "on") == 0;
    }
  }

  s.dirty = true;
}

void bambuSetup() {
  snprintf(topicReport, sizeof(topicReport), "device/%s/report", PRINTER_SERIAL);
  snprintf(topicRequest, sizeof(topicRequest), "device/%s/request", PRINTER_SERIAL);

  // The printer uses a self-signed certificate.
  tlsClient.setInsecure();
  mqtt.setServer(PRINTER_IP, 8883);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setSocketTimeout(5);
  mqtt.setKeepAlive(30);
  mqtt.setCallback(onMqttMessage);
}

void bambuLoop() {
  BambuConn prevConn = printerState.conn;

  if (WiFi.status() != WL_CONNECTED) {
    printerState.conn = BambuConn::WIFI_CONNECTING;
    if (printerState.conn != prevConn) printerState.dirty = true;
    return;
  }

  if (!mqtt.connected()) {
    printerState.conn = BambuConn::MQTT_CONNECTING;
    if (printerState.conn != prevConn) printerState.dirty = true;

    uint32_t now = millis();
    if (now - lastConnectAttempt < RECONNECT_INTERVAL_MS) return;
    lastConnectAttempt = now;

    char clientId[24];
    snprintf(clientId, sizeof(clientId), "cyd-%08x", (uint32_t)ESP.getEfuseMac());
    Serial.printf("[bambu] connecting to %s:8883...\n", PRINTER_IP);
    if (mqtt.connect(clientId, "bblp", PRINTER_ACCESS_CODE)) {
      Serial.println("[bambu] connected");
      mqtt.subscribe(topicReport);
      printerState.conn = BambuConn::CONNECTED;
      printerState.dirty = true;
      requestPushAll();
    } else {
      Serial.printf("[bambu] connect failed, rc=%d\n", mqtt.state());
    }
    return;
  }

  printerState.conn = BambuConn::CONNECTED;
  if (printerState.conn != prevConn) printerState.dirty = true;
  mqtt.loop();
}
