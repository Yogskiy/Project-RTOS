#include "comm.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>

// ============ WIFI ============

int initWiFi(const char *ssid, const char *password) {
    if (ssid == NULL || password == NULL) return -1;
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    return (WiFi.status() == WL_CONNECTED) ? 0 : -1;
}

int isWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED) ? 1 : 0;
}

int getWiFiSignalStrength() {
    return (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
}

const char* getESP32IPAddress() {
    static String ip_string;
    ip_string = WiFi.localIP().toString();
    return ip_string.c_str();
}

// ============ INTERNAL HELPER ============

/**
 * Perform an HTTP POST with a JSON body.
 * Returns the HTTP status code, or -1 on connection error.
 * response_body is filled (up to resp_len-1 bytes) only on success.
 */
static int httpPostJSON(const char *url,
                        const char *json_body,
                        char       *response_body,
                        int         resp_len) {
    if (!isWiFiConnected()) return -1;

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    int code = http.POST((uint8_t *)json_body, strlen(json_body));

    if (code > 0 && response_body != NULL) {
        String resp = http.getString();
        strncpy(response_body, resp.c_str(), resp_len - 1);
        response_body[resp_len - 1] = '\0';
    }

    http.end();
    return code;
}

// ============ API CALLS ============

int verifyUIDWithServer(const char *uid_hex, ServerAuthResponse *response) {
    if (uid_hex == NULL || response == NULL) return -1;

    // Build request JSON
    JsonDocument req_doc;
    req_doc["uid"] = uid_hex;
    char req_body[64];
    serializeJson(req_doc, req_body, sizeof(req_body));

    // Send POST
    char resp_body[256] = {0};
    int http_code = httpPostJSON(API_VERIFY_UID, req_body, resp_body, sizeof(resp_body));

    if (http_code != 200) {
        Serial.printf("[HTTP] verifyUID failed, code=%d\n", http_code);
        response->authorized = 0;
        response->user_id = 0;
        response->user_name[0] = '\0';
        return -1;
    }

    // Parse response
    JsonDocument resp_doc;
    DeserializationError err = deserializeJson(resp_doc, resp_body);
    if (err) {
        Serial.println("[HTTP] verifyUID: JSON parse error");
        return -1;
    }

    response->authorized = resp_doc["authorized"] | false;
    response->user_id    = resp_doc["user_id"]    | 0;
    strlcpy(response->user_name,
            resp_doc["user_name"] | "",
            sizeof(response->user_name));

    return 0;
}

int updateUIDOnServer(int user_id, const char *new_uid_hex) {
    if (new_uid_hex == NULL) return -1;

    JsonDocument req_doc;
    req_doc["user_id"] = user_id;
    req_doc["new_uid"] = new_uid_hex;
    char req_body[128];
    serializeJson(req_doc, req_body, sizeof(req_body));

    int http_code = httpPostJSON(API_UPDATE_UID, req_body, NULL, 0);
    if (http_code != 200) {
        Serial.printf("[HTTP] updateUID failed, code=%d\n", http_code);
        return -1;
    }
    return 0;
}

int logEventToServer(EventLog *event) {
    if (event == NULL) return -1;

    char uid_hex[9];
    snprintf(uid_hex, sizeof(uid_hex), "%02X%02X%02X%02X",
             event->uid[0], event->uid[1],
             event->uid[2], event->uid[3]);

    JsonDocument req_doc;
    req_doc["type"]      = (int)event->type;
    req_doc["timestamp"] = event->timestamp;
    req_doc["uid"]       = uid_hex;
    req_doc["user_name"] = event->user_name;
    req_doc["result"]    = event->result;
    req_doc["message"]   = event->message;

    char req_body[512];
    serializeJson(req_doc, req_body, sizeof(req_body));

    int http_code = httpPostJSON(API_LOG_EVENT, req_body, NULL, 0);
    if (http_code != 200) {
        Serial.printf("[HTTP] logEvent failed, code=%d\n", http_code);
        return -1;
    }
    return 0;
}

int sendHeartbeat() {
    if (!isWiFiConnected()) return -1;

    HTTPClient http;
    http.begin(API_HEARTBEAT);
    http.setTimeout(HTTP_TIMEOUT_MS);
    int code = http.GET();
    http.end();

    return (code == 200) ? 0 : -1;
}