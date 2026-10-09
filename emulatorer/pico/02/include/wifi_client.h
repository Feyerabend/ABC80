#pragma once
// wifi_client.h — WiFi STA + blocking HTTP client for ABC80 Pico
//
// Connects to the PicoFS access point and provides simple HTTP operations
// used by sd_device.c (SAVE/LOAD) and the monitor file commands.
//
// All HTTP calls are blocking — the caller stalls until the response arrives
// or the timeout fires.  This is acceptable because they are only called from
// SD: device trap handlers, which already pause Z80 execution.
//
// Thread safety: the lwIP stack runs in threadsafe_background mode (interrupt
// context).  All lwIP API calls from main context must be wrapped in
// cyw43_arch_lwip_begin() / cyw43_arch_lwip_end().

#include <stdint.h>
#include <stdbool.h>

// Hardcoded PicoFS credentials — both Picos are dedicated to each other.
#define PICOFS_SSID   "PicoFS"
#define PICOFS_PASS   "pico1234"
#define PICOFS_IP     "192.168.4.1"
#define PICOFS_PORT   80

// Return values for http_* functions
#define HTTP_OK          200
#define HTTP_NOT_FOUND   404
#define HTTP_ERR_NETWORK  -1   // connect / send / recv failed
#define HTTP_ERR_TIMEOUT  -2   // no response within deadline

// ---------------------------------------------------------------------------

// WiFi connection state — readable from any module.
typedef enum {
    WIFI_STATE_OFF        = 0,  // not initialised
    WIFI_STATE_CONNECTING = 1,  // init done, association in progress
    WIFI_STATE_UP         = 2,  // associated and link is up
    WIFI_STATE_DOWN       = 3,  // was up, link lost
} wifi_state_t;

// Current state (never blocks).
wifi_state_t    wifi_get_state(void);

// Short human-readable label: "off" / "connecting" / "up" / "down".
const char     *wifi_state_str(void);

// Initialise WiFi hardware and connect to the PicoFS AP.
// Blocks for up to ~15 s.  Returns true on success.
// Safe to call multiple times — if already UP, returns true immediately.
bool wifi_connect(void);

// Drop association.  Hardware stays initialised; wifi_connect() can reconnect.
// Returns true if the leave call succeeded (or was already disconnected).
bool wifi_disconnect(void);

// True if currently associated and link is up.
bool wifi_ready(void);

// ---------------------------------------------------------------------------
// Blocking HTTP operations.
//
// path_query : URL path + query string, e.g. "/read?path=TEST.BAC&off=0&n=2048"
// body_out   : buffer for the response body (may be NULL if caller doesn't need it)
// buf_size   : size of body_out buffer
// len_out    : if not NULL, receives actual body length written
//
// Returns HTTP status code (200, 404, 500 …) or HTTP_ERR_* on network error.

int http_get(const char *path_query,
             uint8_t *body_out, uint16_t buf_size, uint16_t *len_out);

int http_post(const char *path_query,
              const uint8_t *body, uint16_t body_len,
              uint8_t *resp_out, uint16_t resp_size, uint16_t *resp_len_out);

int http_delete(const char *path_query);
