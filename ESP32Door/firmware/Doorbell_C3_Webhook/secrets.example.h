#pragma once
// Template. Copy this file to secrets.h (which is gitignored) and fill in your values.

#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASS       "YOUR_WIFI_PASSWORD"
// Home Assistant webhook (no auth — keep the id private). Match the webhook_id
// in your HA automation (packages/doorbell.yaml).
#define HA_WEBHOOK_URL  "http://HA_IP:8123/api/webhook/your-webhook-id"
