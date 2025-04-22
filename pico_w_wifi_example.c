#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/i2c.h"

// Definições
#define LED_PIN 12         // Pino do LED (não será utilizado)
#define BUTTON1_PIN 5      // Pino do botão 1
#define BUTTON2_PIN 6      // Pino do botão 2
#define WIFI_SSID ""  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "" // Substitua pela senha da sua rede Wi-Fi

#define JOYSTICK_X 27      // Pino analógico para movimentação X
#define JOYSTICK_Y 26      // Pino analógico para movimentação Y
#define BUTTON_PIN 6       // Pino digital para o botão de confirmação

#define I2C_PORT i2c1
#define SDA_PIN 14
#define SCL_PIN 15
#define OLED_ADDR 0x3C

// Estado dos botões (inicialmente sem mensagens)
char button1_message[50] = "Nenhum evento no botão 1";
char button2_message[50] = "Nenhum evento no botão 2";

// Buffer para resposta HTTP
char http_response[1024];

// Função para mapear os valores do joystick para direções
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
    return "Centro"; // Posição neutra
}


// Função para criar a resposta HTTP
void create_http_response(int joystick_x, int joystick_y) {
    const char* direction = map_joystick_to_direction(joystick_x, joystick_y);
    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "  <meta charset=\"UTF-8\">"
             "  <title>Monitoramento de Joystick e Botões</title>"
             "</head>"
             "<body>"
             "  <h1>Monitoramento de Joystick e Botões</h1>"
             "  <p><a href=\"/update\">Atualizar Estado</a></p>"
             "  <h2>Estado dos Botões:</h2>"
             "  <p>Botão 1: %s</p>"
             "  <p>Botão 2: %s</p>"
             "  <h2>Direção do Joystick:</h2>"
             "  <p>Direção: %s</p>"
             "</body>"
             "</html>\r\n",
             button1_message, button2_message, direction);
}

// Função de callback para processar requisições HTTP
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Cliente fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Processa a requisição HTTP
    char *request = (char *)p->payload;

    // Atualiza o conteúdo da página com base no estado dos botões e direção do joystick
    adc_select_input(1); // Seleciona o canal do joystick X
    int joystick_x = adc_read();
    adc_select_input(0); // Seleciona o canal do joystick Y
    int joystick_y = adc_read();

    create_http_response(joystick_x, joystick_y);

    // Envia a resposta HTTP
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);  // Associa o callback HTTP
    return ERR_OK;
}

// Função de setup do servidor TCP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    // Liga o servidor na porta 80
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);  // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback);  // Associa o callback de conexão

    printf("Servidor HTTP rodando na porta 80...\n");
}

// Função para monitorar o estado dos botões
void monitor_buttons() {
    static bool button1_last_state = false;
    static bool button2_last_state = false;

    bool button1_state = !gpio_get(BUTTON1_PIN); // Botão pressionado = LOW
    bool button2_state = !gpio_get(BUTTON2_PIN);

    if (button1_state != button1_last_state) {
        button1_last_state = button1_state;
        if (button1_state) {
            snprintf(button1_message, sizeof(button1_message), "Botão 1 foi pressionado!");
            printf("Botão 1 pressionado\n");
        } else {
            snprintf(button1_message, sizeof(button1_message), "Botão 1 foi solto!");
            printf("Botão 1 solto\n");
        }
    }

    if (button2_state != button2_last_state) {
        button2_last_state = button2_state;
        if (button2_state) {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi pressionado!");
            printf("Botão 2 pressionado\n");
        } else {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi solto!");
            printf("Botão 2 solto\n");
        }
    }
}

int main() {
    stdio_init_all();  // Inicializa a saída padrão
    sleep_ms(10000);
    printf("Iniciando servidor HTTP\n");

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    } else {
        printf("Conectado.\n");
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP: %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    // Configura os botões
    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    // Inicializa os pinos do joystick (com ADC)
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);

    // Inicia o servidor HTTP
    start_http_server();

    // Loop principal
    while (true) {
        monitor_buttons();
        sleep_ms(100); // Reduz o uso da CPU
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}

