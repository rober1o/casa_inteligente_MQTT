#ifndef CASA_INTELIGENTE
#define CASA_INTELIGENTE

// ============================
//  INCLUDES (BIBLIOTECAS)
// ============================

#include "pico/stdlib.h"     
#include "pico/cyw43_arch.h" 
#include "pico/unique_id.h"  

#include "hardware/gpio.h" 
#include "hardware/irq.h"  
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h" 

#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/matriz_5X5.h"
#include "pio_wave.pio.h"

#include "lwip/apps/mqtt.h"      
#include "lwip/apps/mqtt_priv.h" 
#include "lwip/dns.h"            
#include "lwip/altcp_tls.h"      



// ============================
//  DEFINIÇÕES (DEFINES & CONST)
// ============================

#define WIFI_SSID "BORGES"          // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASSWORD "gomugomu"    // Substitua pela senha da sua rede Wi-Fi
#define MQTT_SERVER "192.168.1.153" // Substitua pelo endereço do host - broket MQTT: Ex: 192.168.1.107
#define MQTT_USERNAME "roberto"     // Substitua pelo nome da host MQTT - Username
#define MQTT_PASSWORD "embarcatech" // Substitua pelo Password da host MQTT - credencial de acesso - caso exista

// Definição da escala de temperatura
#ifndef TEMPERATURE_UNITS
#define TEMPERATURE_UNITS 'C' // Set to 'F' for Fahrenheit
#endif


//perifericos da BitDogLab
#define LED_BLUE_PIN 11
#define LED_GREEN_PIN 12
#define LED_RED_PIN 13
#define BUZZER_PIN 10
#define SERVO_MOTOR_PIN 20
#define SERVO_MIN 1250        
#define SERVO_90_DEGREES 2500 
#define botaoB 6

// I2C - Display OLED
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

// Matriz de LEDs
#define MATRIZ_PIN 7
#define NUM_PIXELS 25
#define BRILHO_PADRAO 50


#define MQTT_TOPIC_LEN 100

// TLS (opcional)
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

// ============================
//  DADOS DO CLIENTE MQTT
// ============================

typedef struct {
    mqtt_client_t *mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

// ============================
//  FUNÇÕES DE DEBUG (LOGS)
// ============================

#ifndef NDEBUG
    #define DEBUG_printf printf
#else
    #define DEBUG_printf(...)
#endif

#define INFO_printf  printf
#define ERROR_printf printf

// ============================
//  CONFIGURAÇÕES MQTT
// ============================

#define TEMP_WORKER_TIME_S   10    // Intervalo de leitura de temperatura (segundos)
#define MQTT_KEEP_ALIVE_S    60    // Tempo de keep-alive

#define MQTT_SUBSCRIBE_QOS   1     // QoS para subscribe
#define MQTT_PUBLISH_QOS     1     // QoS para publish
#define MQTT_PUBLISH_RETAIN  0     // Mensagem retida

#define MQTT_WILL_TOPIC      "/online"
#define MQTT_WILL_MSG        "0"
#define MQTT_WILL_QOS        1

#define MQTT_DEVICE_NAME     "pico"
#define MQTT_UNIQUE_TOPIC    0     // Define se o nome do cliente será incluído no tópico

// ============================
//  VARIAVEIS GLOBAIS
// ============================

PIO pio;       // Instância do PIO
int sm;        // Máquina de estado
ssd1306_t ssd; // Instância do display OLED
bool lampada_quarto = false;
bool lampada_sala = false;
bool alarme = false;
bool modo_viagem = false;
bool porta_open = false;
bool alarme_disparado = false;



// Referencias das implementações

// Leitura de temperatura do microcotrolador
static float read_onboard_temperature(const char unit);

// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err);

// Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);

// CONTROLE LAMPADA SALA
static void controle_lampada_sala(MQTT_CLIENT_DATA_T *state, bool on);

// CONTROLE LAMPADA QUARTO
static void controle_lampada_quarto(MQTT_CLIENT_DATA_T *state, bool on);

// CONTROLE CONTROLE PORTA
static void controle_porta(MQTT_CLIENT_DATA_T *state, bool on);

// CONTROLE ALARME
static void controle_alarme(MQTT_CLIENT_DATA_T *state, bool on);

// CONTROLE MODO VIAGEM
static void controle_modo_viagem(MQTT_CLIENT_DATA_T *state, bool on);

// Publicar temperatura
static void publish_temperature(MQTT_CLIENT_DATA_T *state);

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err);

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err);

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub);

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

// Publicar temperatura
static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t temperature_worker = {.do_work = temperature_worker_fn};

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state);

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);

// Funções de inicialização de periferico
void inicializar_perifericos();
void inicializar_leds(void);
void incializar_servo_motor();
void configurar_matriz_leds();
void inicializar_display_i2c();
void inicializar_pwm_buzzer();
void inicializar_sensor_temperatura();
void atualizar_display();
void monitorar();
void parar_tocar_buzzer();
void desenha_fig(uint32_t *_matriz, uint8_t _intensidade, PIO pio, uint sm);

#endif // CASA_INTELIGENTE_H