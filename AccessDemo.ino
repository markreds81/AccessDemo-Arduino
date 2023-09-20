#include <WiFi.h>
#include <WiFiMulti.h>
#include <RFID.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Button.h>
#include <LED.h>
#include <stdio.h>

#include "Timer.h"

#define URL_MAX_LEN	128
#define CLOCK_PIN    23
#define DATA_PIN     22
#define STATE_PIN		21
#define LED_PIN		18

const uint32_t connectTimeoutMs = 10000;
const char* serviceHost = "192.168.1.111";
const uint16_t servicePort = 8080;

WiFiMulti wifiMulti;
Button doorState(STATE_PIN);
LED led(LED_PIN);
uint8_t wifiStatus = WL_NO_SHIELD;
Timer checkTimer;
Timer workTimer;

void openingRequest() {
	uint8_t tag[8];
	
	RFID.uid(tag, sizeof(tag));
	Serial.printf("[RFID] tag read: %02X%02X%02X%02X%02X%02X%02X%02X\n", tag[0], tag[1], tag[2], tag[3], tag[4], tag[5], tag[6], tag[7]);	
	
	if (wifiStatus == WL_CONNECTED) {
		byte mac[6];
		HTTPClient http;
		char url[URL_MAX_LEN];

		WiFi.macAddress(mac);
		snprintf(url, sizeof(url), "http://%s:%u/api/v1/can-open?door=%02X%02X%02X%02X%02X%02X&keycode=%02X%02X%02X%02X%02X%02X%02X%02X",
			serviceHost, servicePort,
			mac[0], mac[1], mac[2],	mac[3], mac[4], mac[5],
			tag[0], tag[1], tag[2], tag[3], tag[4], tag[5],	tag[6], tag[7]);
		Serial.printf("[HTTP] Connecting to %s\n", url);
		http.begin(url);
		int httpCode = http.GET();
		if (httpCode > 0) {
			Serial.printf("[HTTP] GET... code: %d\n", httpCode);
			if (httpCode == HTTP_CODE_OK) {
				StaticJsonDocument<128> doc;
				String payload = http.getString();
				Serial.print("[HTTP] ");
				Serial.println(payload);
				deserializeJson(doc, payload);
				bool allowed = doc["allowed"];
				int workTime = doc["workTime"];
				if (allowed) {
					Serial.printf("[DOOR] unlocked for %d seconds\n", workTime);
					workTimer.begin(workTime * 1000L);
				}
			}
		} else {
			Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
		}
		http.end();
	}
}

void stateChanged(bool open) {
	Serial.printf("[DOOR] %s\n", open ? "open" : "closed");

	if (wifiStatus == WL_CONNECTED) {
		byte mac[6];
		HTTPClient http;
		char url[URL_MAX_LEN];
		WiFi.macAddress(mac);
		snprintf(url, sizeof(url), "http://%s:%u/api/v1/state-changed?door=%02X%02X%02X%02X%02X%02X&open=%d",
			serviceHost, servicePort,
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],	(int) open);
		Serial.printf("[HTTP] Connecting to %s\n", url);
		http.begin(url);
		int httpCode = http.GET();
		if (httpCode > 0) {
			Serial.printf("[HTTP] GET... code: %d\n", httpCode);
			if (httpCode == HTTP_CODE_OK) {
				String payload = http.getString();
				Serial.print("[HTTP] ");
				Serial.println(payload);
			}
		} else {
			Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
		}
		http.end();
	}
}

void setup() {
	Serial.begin(115200);
	delay(10);
	WiFi.mode(WIFI_STA);

	wifiMulti.addAP("MY-SSID1", "mypassword");
	wifiMulti.addAP("MY-SSID2", "anotherpassword");
	
	RFID.begin(CLOCK_PIN, DATA_PIN);
	led.begin();
	doorState.begin();
	checkTimer.begin(1000L);
}

void loop() {
	if (checkTimer.expired()) {
		uint8_t status = wifiMulti.run(connectTimeoutMs);
		if (status != wifiStatus) {
			wifiStatus = status;
			switch (status) {
				case WL_CONNECTED:
					Serial.println("[WIFI] Connecting done.");
					Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
					Serial.printf("[WIFI] MAC: %s\n", WiFi.macAddress().c_str());
					Serial.printf("[WIFI] SSID: %s\n", WiFi.SSID().c_str());
					Serial.printf("[WIFI] BSSID: %s\n", WiFi.BSSIDstr().c_str());
					Serial.printf("[WIFI] Channel: %d\n", WiFi.channel());
					break;
				case WL_NO_SSID_AVAIL:
					Serial.println("[WIFI] Connecting Failed AP not found.");
					break;
				case WL_CONNECT_FAILED:
					Serial.println("[WIFI] Connecting Failed.");
				default:
					Serial.printf("[WIFI] Connecting Failed (%d).\n", status);
			}
		}
		checkTimer.reset();
	}

	RFID.loop();

	// richiesta apertura
	if (RFID.status() == RFID_UID_OK) {
		openingRequest();
	}

	// la porta è stata aperta
	if (doorState.pressed()) {
		stateChanged(true);
	}

	// la porta è stata chiusa
	if (doorState.released()) {
		stateChanged(false);
	}

	// stato porta
	if (doorState.state() == Button::PRESSED) {
		led.blink(200);	// aperta
	} else if (workTimer.expired()) {
		led.off();			// chiusa
	} else {
		led.on();			// sbloccata ma non ancora aperta
	}
}
