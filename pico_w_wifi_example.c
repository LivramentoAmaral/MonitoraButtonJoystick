#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include "hardware/adc.h"

// Definições de pinos
#define BUTTON1_PIN 5
#define BUTTON2_PIN 6
#define JOYSTICK_X 27
#define JOYSTICK_Y 26

#define WIFI_SSID "PASTELARIA DO GLAUCIO"
#define WIFI_PASS "5276silva"

// Estado dos sensores
char button1_message[50] = "Nenhum evento no botão 1";
char button2_message[50] = "Nenhum evento no botão 2";
char temperature_message[50] = "Temperatura: N/A";
char joystick_direction[50] = "Centro";

char http_response[1024];

// Protótipos
void monitor_sensors();

// Função para mapear joystick para direção
const char* map_joystick_to_direction(int x, int y) {
    const int threshold_low = 1500;
    const int threshold_high = 2500;

    if (x < threshold_low && y > threshold_high) return "Noroeste";
    if (x > threshold_high && y > threshold_high) return "Nordeste";
    if (x < threshold_low && y < threshold_low) return "Sudoeste";
    if (x > threshold_high && y < threshold_low) return "Sudeste";
    if (x < threshold_low) return "Oeste";
    if (x > threshold_high) return "Leste";
    if (y > threshold_high) return "Norte";
    if (y < threshold_low) return "Sul";
    return "Centro";
}

// Leitura do sensor de temperatura interna
float read_temperature_celsius() {
    adc_select_input(4);  // Sensor interno
    uint16_t raw = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

// Cria a resposta HTTP
void create_http_response() {
    snprintf(http_response, sizeof(http_response),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
            "<!DOCTYPE html>"
            "<html>"
            "<head><meta charset=\"UTF-8\"><title>Status do Sistema</title></head>"
            "<body>"
            "<h1>Monitoramento</h1>"
            "<p><a href=\"/update\">Atualizar</a></p>"
            "<h2>Botões:</h2>"
            "<p>Botão 1: %s</p>"
            "<p>Botão 2: %s</p>"
            "<h2>Temperatura:</h2>"
            "<p>%s</p>"
            "<h2>Joystick:</h2>"
            "<p>Direção: %s</p>"
            "</body></html>\r\n",
            button1_message, button2_message, temperature_message, joystick_direction);
}

// Callback HTTP
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    monitor_sensors();  // Atualiza todos os sensores
    create_http_response();

    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    pbuf_free(p);
    return ERR_OK;
}

// Callback de nova conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

// Inicia servidor HTTP
static void start_http_server() {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

// Monitoramento dos sensores e exibição no terminal
void monitor_sensors() {
    static bool b1_last = false, b2_last = false;
    bool b1 = !gpio_get(BUTTON1_PIN);
    bool b2 = !gpio_get(BUTTON2_PIN);

    if (b1 != b1_last) {
        b1_last = b1;
        snprintf(button1_message, sizeof(button1_message), b1 ? "Botão 1 pressionado!" : "Botão 1 solto!");
    }

    if (b2 != b2_last) {
        b2_last = b2;
        snprintf(button2_message, sizeof(button2_message), b2 ? "Botão 2 pressionado!" : "Botão 2 solto!");
    }

    // Leitura do joystick
    adc_select_input(1);
    int joy_x = adc_read();
    adc_select_input(0);
    int joy_y = adc_read();
    const char* direction = map_joystick_to_direction(joy_x, joy_y);
    snprintf(joystick_direction, sizeof(joystick_direction), "%s", direction);

    // Leitura da temperatura
    float temp = read_temperature_celsius();
    snprintf(temperature_message, sizeof(temperature_message), "Temperatura: %.2f °C", temp);

    // Exibe painel no terminal
    printf("\n----------------------\n");
    printf("STATUS DO SISTEMA:\n");
    printf("----------------------\n");
    printf("%s\n", button1_message);
    printf("%s\n", button2_message);
    printf("%s\n", temperature_message);
    printf("Joystick X: %d | Y: %d\n", joy_x, joy_y);
    printf("Direção: %s\n", joystick_direction);
    printf("----------------------\n");
}

// Função principal
int main() {
    stdio_init_all();
    sleep_ms(10000);
    printf("Iniciando sistema...\n");

    if (cyw43_arch_init()) {
        printf("Falha na inicialização do Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Tentando conectar ao Wi-Fi...\n");
        sleep_ms(5000);
    }

    printf("Conectado ao Wi-Fi.\n");
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    printf("IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

    // Inicializa GPIOs
    gpio_init(BUTTON1_PIN); gpio_set_dir(BUTTON1_PIN, GPIO_IN); gpio_pull_up(BUTTON1_PIN);
    gpio_init(BUTTON2_PIN); gpio_set_dir(BUTTON2_PIN, GPIO_IN); gpio_pull_up(BUTTON2_PIN);

    // Inicializa ADCs
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
    adc_set_temp_sensor_enabled(true);

    start_http_server();

    while (true) {
        monitor_sensors();
        sleep_ms(1000);
    }

    return 0;
}
