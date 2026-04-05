// wifi_client.c — WiFi STA + blocking HTTP client for ABC80 Pico
//
// Uses lwIP raw TCP API with cyw43_arch threadsafe_background mode.
// lwIP runs in interrupt context; all lwIP calls from main thread are
// wrapped in cyw43_arch_lwip_begin() / cyw43_arch_lwip_end().
//
// All HTTP functions are blocking — they poll with sleep_ms() which
// yields to the interrupt handler so lwIP can make progress.

#include "wifi_client.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Sizes and timeouts

#define CONNECT_TIMEOUT_MS   8000
#define RECV_TIMEOUT_MS      8000

// rx buffer: response header (~200 B) + max body (CHUNK = 2048 B)
#define RX_BUF_SIZE   4096
// request buffer: request line + headers + max body (CHUNK = 2048 B)
#define REQ_BUF_SIZE  2400

// ---------------------------------------------------------------------------
// Internal state

typedef enum {
    STATE_IDLE = 0,
    STATE_CONNECTING,
    STATE_RECEIVING,   // connected + request sent; waiting for server close
    STATE_DONE,        // server closed; all data in rx_buf
    STATE_ERROR,
} client_state_t;

static struct {
    struct tcp_pcb       *pcb;
    volatile client_state_t state;
    uint8_t               rx_buf[RX_BUF_SIZE];
    uint16_t              rx_len;
} g_http;

static char g_req_buf[REQ_BUF_SIZE];

static wifi_state_t g_wifi_state = WIFI_STATE_OFF;
static bool         g_cyw43_inited = false;

// ---------------------------------------------------------------------------
// TCP callbacks — called from lwIP / interrupt context

static err_t on_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg; (void)pcb;
    if (err != ERR_OK) {
        g_http.state = STATE_ERROR;
        return err;
    }
    // Signal main thread: send the request now.
    g_http.state = STATE_RECEIVING;
    return ERR_OK;
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        g_http.state = STATE_ERROR;
        if (p) pbuf_free(p);
        return err;
    }
    if (p == NULL) {
        // Server closed connection — all data received.
        g_http.state = STATE_DONE;
        return ERR_OK;
    }
    // Accumulate payload into rx_buf (silently clamp if overflow — shouldn't happen).
    for (struct pbuf *q = p; q; q = q->next) {
        uint16_t avail = RX_BUF_SIZE - g_http.rx_len;
        uint16_t copy  = (q->len < avail) ? (uint16_t)q->len : avail;
        if (copy) memcpy(g_http.rx_buf + g_http.rx_len, q->payload, copy);
        g_http.rx_len += copy;
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void on_err(void *arg, err_t err) {
    (void)arg; (void)err;
    if (g_http.pcb == NULL) return;  // deliberate tcp_abort from cleanup — not a real error
    g_http.pcb   = NULL;   // lwIP already freed the PCB on error
    g_http.state = STATE_ERROR;
}

// ---------------------------------------------------------------------------
// Response parsing helpers

// "HTTP/1.1 200 OK\r\n..." → 200, or HTTP_ERR_NETWORK on parse failure.
static int parse_status(const uint8_t *buf, uint16_t len) {
    for (uint16_t i = 0; i + 4 < len; i++) {
        if (buf[i] == ' ') {
            int code = 0, digits = 0;
            for (uint16_t j = i + 1; j < len && buf[j] >= '0' && buf[j] <= '9'; j++, digits++)
                code = code * 10 + (buf[j] - '0');
            if (digits == 3 && code >= 100 && code <= 599) return code;
            break;
        }
    }
    return HTTP_ERR_NETWORK;
}

// Find body start after \r\n\r\n, set *body_len.
static const uint8_t *find_body(const uint8_t *buf, uint16_t len, uint16_t *body_len) {
    for (uint16_t i = 0; i + 3 < len; i++) {
        if (buf[i]=='\r' && buf[i+1]=='\n' && buf[i+2]=='\r' && buf[i+3]=='\n') {
            uint16_t start = i + 4;
            *body_len = len - start;
            return buf + start;
        }
    }
    *body_len = 0;
    return NULL;
}

// ---------------------------------------------------------------------------
// Core blocking HTTP request
//
// Sends `request` (req_len bytes) over TCP, waits for server to close,
// then parses the HTTP status code and optional body.
//
// Returns HTTP status code (200, 404, ...) or HTTP_ERR_*.

static int do_http(const char *request, uint16_t req_len,
                   uint8_t *body_out, uint16_t buf_size, uint16_t *len_out) {
    // Reset shared state
    g_http.state  = STATE_CONNECTING;
    g_http.rx_len = 0;
    g_http.pcb    = NULL;

    ip_addr_t server_ip;
    IP4_ADDR(&server_ip, 192, 168, 4, 1);

    // Allocate PCB and start connect
    cyw43_arch_lwip_begin();
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!pcb) {
        cyw43_arch_lwip_end();
        return HTTP_ERR_NETWORK;
    }
    g_http.pcb = pcb;
    tcp_arg(pcb,  NULL);
    tcp_err(pcb,  on_err);
    tcp_recv(pcb, on_recv);
    err_t err = tcp_connect(pcb, &server_ip, PICOFS_PORT, on_connected);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        cyw43_arch_lwip_begin();
        if (g_http.pcb) { tcp_abort(g_http.pcb); g_http.pcb = NULL; }
        cyw43_arch_lwip_end();
        return HTTP_ERR_NETWORK;
    }

    // Wait for STATE_RECEIVING (set by on_connected) or error
    uint32_t deadline = to_ms_since_boot(get_absolute_time()) + CONNECT_TIMEOUT_MS;
    while (g_http.state == STATE_CONNECTING) {
        if (to_ms_since_boot(get_absolute_time()) > deadline) {
            cyw43_arch_lwip_begin();
            if (g_http.pcb) { tcp_abort(g_http.pcb); g_http.pcb = NULL; }
            cyw43_arch_lwip_end();
            printf("WiFi: connect timeout\n");
            return HTTP_ERR_TIMEOUT;
        }
        sleep_ms(10);
    }
    if (g_http.state != STATE_RECEIVING) {
        printf("WiFi: connect failed (state=%d)\n", (int)g_http.state);
        return HTTP_ERR_NETWORK;
    }

    // Send request
    cyw43_arch_lwip_begin();
    err = tcp_write(g_http.pcb, request, req_len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) err = tcp_output(g_http.pcb);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        cyw43_arch_lwip_begin();
        if (g_http.pcb) { tcp_abort(g_http.pcb); g_http.pcb = NULL; }
        cyw43_arch_lwip_end();
        printf("WiFi: send failed (%d)\n", (int)err);
        return HTTP_ERR_NETWORK;
    }

    // Wait for STATE_DONE (on_recv p==NULL = server closed) or error
    deadline = to_ms_since_boot(get_absolute_time()) + RECV_TIMEOUT_MS;
    while (g_http.state == STATE_RECEIVING) {
        if (to_ms_since_boot(get_absolute_time()) > deadline) {
            cyw43_arch_lwip_begin();
            if (g_http.pcb) { tcp_abort(g_http.pcb); g_http.pcb = NULL; }
            cyw43_arch_lwip_end();
            printf("WiFi: recv timeout\n");
            return HTTP_ERR_TIMEOUT;
        }
        sleep_ms(10);
    }

    // Abort PCB if still open — tcp_abort frees the slot immediately so the
    // next BL_UT call can allocate a new PCB.  (tcp_close would leave the PCB
    // in LAST_ACK state until the server ACKs the FIN, exhausting the small
    // MEMP_NUM_TCP_PCB pool across the multiple sequential HTTP POSTs that
    // BAS SAVE generates.)  We null g_http.pcb first so on_err becomes a no-op.
    cyw43_arch_lwip_begin();
    if (g_http.pcb) {
        struct tcp_pcb *pcb = g_http.pcb;
        g_http.pcb = NULL;   // must come before tcp_abort so on_err is a no-op
        tcp_abort(pcb);
    }
    cyw43_arch_lwip_end();

    if (g_http.state != STATE_DONE) {
        printf("WiFi: request failed (state=%d)\n", (int)g_http.state);
        return HTTP_ERR_NETWORK;
    }

    // Parse HTTP status code
    int status = parse_status(g_http.rx_buf, g_http.rx_len);

    // Extract body
    if (body_out && buf_size > 0) {
        uint16_t blen = 0;
        const uint8_t *body = find_body(g_http.rx_buf, g_http.rx_len, &blen);
        if (body && blen > 0) {
            uint16_t copy = (blen < buf_size) ? blen : buf_size;
            memcpy(body_out, body, copy);
            if (len_out) *len_out = copy;
        } else {
            if (len_out) *len_out = 0;
        }
    } else {
        if (len_out) *len_out = 0;
    }

    return status;
}

// ---------------------------------------------------------------------------
// Public API — state, connect, disconnect, ready

wifi_state_t wifi_get_state(void) {
    // Refresh UP/DOWN dynamically so callers always see current link state.
    if (g_wifi_state == WIFI_STATE_UP || g_wifi_state == WIFI_STATE_DOWN) {
        bool up = (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA)
                   == CYW43_LINK_UP);
        g_wifi_state = up ? WIFI_STATE_UP : WIFI_STATE_DOWN;
    }
    return g_wifi_state;
}

const char *wifi_state_str(void) {
    switch (wifi_get_state()) {
        case WIFI_STATE_OFF:        return "off";
        case WIFI_STATE_CONNECTING: return "connecting";
        case WIFI_STATE_UP:         return "up";
        case WIFI_STATE_DOWN:       return "down";
        default:                    return "?";
    }
}

bool wifi_connect(void) {
    if (wifi_get_state() == WIFI_STATE_UP) return true;

    if (!g_cyw43_inited) {
        if (cyw43_arch_init()) {
            printf("WiFi: cyw43_arch_init failed\n");
            return false;
        }
        g_cyw43_inited = true;
        cyw43_arch_enable_sta_mode();
    }

    g_wifi_state = WIFI_STATE_CONNECTING;
    printf("WiFi: connecting to \"" PICOFS_SSID "\" ...\n");

    // Try WPA2-AES first, then WPA2-mixed as fallback (MicroPython AP default
    // varies across firmware versions).
    int rc = cyw43_arch_wifi_connect_timeout_ms(PICOFS_SSID, PICOFS_PASS,
                                                CYW43_AUTH_WPA2_AES_PSK,
                                                10000);
    if (rc != 0) {
        printf("WiFi: WPA2-AES failed (%d), trying mixed...\n", rc);
        rc = cyw43_arch_wifi_connect_timeout_ms(PICOFS_SSID, PICOFS_PASS,
                                                CYW43_AUTH_WPA2_MIXED_PSK,
                                                10000);
    }
    if (rc != 0) {
        g_wifi_state = WIFI_STATE_DOWN;
        printf("WiFi: connect failed (%d)\n", rc);
        return false;
    }

    g_wifi_state = WIFI_STATE_UP;
    printf("WiFi: connected  ip=%s\n",
           ip4addr_ntoa(netif_ip4_addr(netif_default)));
    return true;
}

bool wifi_disconnect(void) {
    if (g_wifi_state == WIFI_STATE_OFF) return true;
    int rc = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    g_wifi_state = WIFI_STATE_DOWN;
    printf("WiFi: disconnected (%d)\n", rc);
    return (rc == 0);
}

bool wifi_ready(void) {
    return wifi_get_state() == WIFI_STATE_UP;
}

// ---------------------------------------------------------------------------
// Public API — HTTP operations

int http_get(const char *path_query,
             uint8_t *body_out, uint16_t buf_size, uint16_t *len_out) {
    if (wifi_get_state() != WIFI_STATE_UP) return HTTP_ERR_NETWORK;
    int n = snprintf(g_req_buf, sizeof(g_req_buf),
                     "GET %s HTTP/1.1\r\n"
                     "Host: " PICOFS_IP "\r\n"
                     "Connection: close\r\n\r\n",
                     path_query);
    if (n <= 0 || n >= (int)sizeof(g_req_buf)) return HTTP_ERR_NETWORK;
    return do_http(g_req_buf, (uint16_t)n, body_out, buf_size, len_out);
}

int http_post(const char *path_query,
              const uint8_t *body, uint16_t body_len,
              uint8_t *resp_out, uint16_t resp_size, uint16_t *resp_len_out) {
    if (wifi_get_state() != WIFI_STATE_UP) return HTTP_ERR_NETWORK;
    int hdr_len = snprintf(g_req_buf, sizeof(g_req_buf),
                           "POST %s HTTP/1.1\r\n"
                           "Host: " PICOFS_IP "\r\n"
                           "Content-Type: application/octet-stream\r\n"
                           "Content-Length: %u\r\n"
                           "Connection: close\r\n\r\n",
                           path_query, (unsigned)body_len);
    if (hdr_len <= 0 || hdr_len + body_len > (int)sizeof(g_req_buf))
        return HTTP_ERR_NETWORK;
    if (body && body_len > 0)
        memcpy(g_req_buf + hdr_len, body, body_len);
    return do_http(g_req_buf, (uint16_t)(hdr_len + body_len),
                   resp_out, resp_size, resp_len_out);
}

int http_delete(const char *path_query) {
    if (wifi_get_state() != WIFI_STATE_UP) return HTTP_ERR_NETWORK;
    int n = snprintf(g_req_buf, sizeof(g_req_buf),
                     "DELETE %s HTTP/1.1\r\n"
                     "Host: " PICOFS_IP "\r\n"
                     "Connection: close\r\n\r\n",
                     path_query);
    if (n <= 0 || n >= (int)sizeof(g_req_buf)) return HTTP_ERR_NETWORK;
    return do_http(g_req_buf, (uint16_t)n, NULL, 0, NULL);
}
