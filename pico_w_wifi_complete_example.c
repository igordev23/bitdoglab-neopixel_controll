#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h"

#define LED_PIN 7        
#define BUTTON1_PIN 5
#define BUTTON2_PIN 6
#define WIFI_SSID "Casa"
#define WIFI_PASS "KYOUSOU34"
#define LED_COUNT 25
#define MATRIX_WIDTH 5
#define MATRIX_HEIGHT 5

typedef struct {
    uint8_t G, R, B;
} npLED_t;

npLED_t leds[LED_COUNT]; 
PIO np_pio;
uint sm;

char http_response[2048];

void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    memset(leds, 0, sizeof(leds));
}

void npSetLED(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
        int index = (y % 2 == 0) ? (y * MATRIX_WIDTH + x) : (y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x));
        leds[index].R = r;
        leds[index].G = g;
        leds[index].B = b;
        npWrite();
    }
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, (leds[i].G << 16) | (leds[i].R << 8) | leds[i].B);
    }
    sleep_us(100);
}

err_t http_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char request[1024] = {0};
    pbuf_copy_partial(p, request, sizeof(request), 0);
    pbuf_free(p);

    int x, y, r, g, b;
    if (sscanf(request, "GET /led/set?x=%d&y=%d&r=%d&g=%d&b=%d", &x, &y, &r, &g, &b) == 5) {
        npSetLED(x, y, r, g, b);
    }

    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head><meta charset=\"UTF-8\"><title>Controle da Matriz</title></head>"
             "<body>"
             "<h1>Controle da Matriz de LEDs</h1>"
             "<p><a href=\"/led/set?x=0&y=0&r=255&g=0&b=0\">Ligar LED (0,0) Vermelho</a></p>"
             "<p><a href=\"/led/set?x=1&y=1&r=0&g=255&b=0\">Ligar LED (1,1) Verde</a></p>"
             "</body>"
             "</html>\r\n");

    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    return ERR_OK;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_server_recv);
    return ERR_OK;
}

void start_http_server() {
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, accept_callback);
}

int main() {
    stdio_init_all();
    npInit(LED_PIN);
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000);
    start_http_server();

    while (true) {
        tight_loop_contents();
    }
}
