#include "casa_inteligente.h"

int main(void)
{
    inicializar_pwm_buzzer();
    inicializar_display_i2c();
    configurar_matriz_leds();
    inicializar_leds();
    incializar_servo_motor();
    inicializar_sensor_temperatura();

    desenha_fig(matriz_apagada, BRILHO_PADRAO, pio, sm);

    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

    INFO_printf("mqtt client starting\n");

    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    // Cria registro com os dados do cliente
    static MQTT_CLIENT_DATA_T state;

    // Inicializa a arquitetura do cyw43
    if (cyw43_arch_init())
    {
        panic("Failed to inizialize CYW43");
    }

    // Usa identificador único da placa
    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for (int i = 0; i < sizeof(unique_id_buf) - 1; i++)
    {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    // Gera nome único, Ex: pico1234
    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf) - 1];
    memcpy(&client_id_buf[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&client_id_buf[sizeof(MQTT_DEVICE_NAME) - 1], unique_id_buf, sizeof(unique_id_buf) - 1);
    client_id_buf[sizeof(client_id_buf) - 1] = 0;
    INFO_printf("Device name %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S; // Keep alive in sec
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;
#endif
    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // TLS enabled
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    // This confirms the indentity of the server and the client
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
                                                                                client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    WARN_printf("Warning: tls without verification is insecure\n");
#endif
#else
    state->client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    WARN_printf("Warning: tls without a certificate is insecure\n");
#endif
#endif

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        panic("Failed to connect");
    }
    INFO_printf("\nConnected to Wifi\n");

    // Faz um pedido de DNS para o endereço IP do servidor MQTT
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    // Se tiver o endereço, inicia o cliente
    if (err == ERR_OK)
    {
        start_client(&state);
    }
    else if (err != ERR_INPROGRESS)
    { // ERR_INPROGRESS means expect a callback
        panic("dns request failed");
    }

    // Loop condicionado a conexão mqtt
    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst))
    {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));
        monitorar(); // verifica necessidade de soar o alarme ou não, além de atualizar o display
        sleep_ms(100);
    }

    INFO_printf("mqtt client exiting\n");

    return 0;
}

// ========================================================
//         FUNÇÕES DE INICIALIZAÇÃO DOS PERIFERICOS
// ========================================================

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void inicializar_leds()
{
    // Configuração dos LEDs como saída
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);
}

void inicializar_sensor_temperatura()
{
    // Inicializa o ADC
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
}

void incializar_servo_motor() // inicializa o pwm do servo motor
{
    gpio_set_function(SERVO_MOTOR_PIN, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(SERVO_MOTOR_PIN); // Agora está correto!

    pwm_set_wrap(slice_num, 20000);    // 20 ms período (50Hz)
    pwm_set_clkdiv(slice_num, 125.0f); // Clock de 1MHz
    pwm_set_enabled(slice_num, true);
}

void configurar_matriz_leds() // FUNÇÃO PARA CONFIGURAR O PIO PARA USAR NA MATRIZ DE LEDS
{
    bool clock_setado = set_sys_clock_khz(133000, false);
    pio = pio0;
    sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &Matriz_5x5_program);
    Matriz_5x5_program_init(pio, sm, offset, MATRIZ_PIN);
}

void inicializar_display_i2c()
{
    // Inicializa I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);

    // Limpa o display
    ssd1306_fill(&ssd, false);

    // Escreve texto
    ssd1306_fill(&ssd, false);                      // limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Moldura
    ssd1306_draw_string(&ssd, "INICIANDO...", 20, 30);
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Envia os dados para o display
    ssd1306_send_data(&ssd);
}

void inicializar_pwm_buzzer() // inicializa pwm do buzzer
{
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);

    pwm_set_clkdiv(slice_num, 10.0f);                                      // Reduz clock base para 12.5 MHz
    pwm_set_wrap(slice_num, 31250);                                        // 12.5 MHz / 31250 = 400 Hz
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(BUZZER_PIN), 15625); // 50% duty cycle
    pwm_set_enabled(slice_num, false);                                     // Começa desligado
}

// ========================================================
//              FUNÇÕES AUXILIARES DO SISTEMA
// ========================================================

void desenha_fig(uint32_t *_matriz, uint8_t _intensidade, PIO pio, uint sm) // FUNÇÃO DESENHAR A LAMPADA DA SALA
{
    uint32_t pixel = 0;
    uint8_t r, g, b;

    for (int i = 24; i > 19; i--) // Linha 1
    {
        pixel = _matriz[i];
        b = ((pixel >> 16) & 0xFF) * (_intensidade / 100.00); // Isola os 8 bits mais significativos (azul)
        g = ((pixel >> 8) & 0xFF) * (_intensidade / 100.00);  // Isola os 8 bits intermediários (verde)
        r = (pixel & 0xFF) * (_intensidade / 100.00);         // Isola os 8 bits menos significativos (vermelho)
        pixel = 0;
        pixel = (g << 16) | (r << 8) | b;
        pio_sm_put_blocking(pio, sm, pixel << 8u);
    }

    for (int i = 15; i < 20; i++) // Linha 2
    {
        pixel = _matriz[i];
        b = ((pixel >> 16) & 0xFF) * (_intensidade / 100.00); // Isola os 8 bits mais significativos (azul)
        g = ((pixel >> 8) & 0xFF) * (_intensidade / 100.00);  // Isola os 8 bits intermediários (verde)
        r = (pixel & 0xFF) * (_intensidade / 100.00);         // Isola os 8 bits menos significativos (vermelho)
        pixel = 0;
        pixel = (b << 16) | (r << 8) | g;
        pixel = (g << 16) | (r << 8) | b;
        pio_sm_put_blocking(pio, sm, pixel << 8u);
    }

    for (int i = 14; i > 9; i--) // Linha 3
    {
        pixel = _matriz[i];
        b = ((pixel >> 16) & 0xFF) * (_intensidade / 100.00); // Isola os 8 bits mais significativos (azul)
        g = ((pixel >> 8) & 0xFF) * (_intensidade / 100.00);  // Isola os 8 bits intermediários (verde)
        r = (pixel & 0xFF) * (_intensidade / 100.00);         // Isola os 8 bits menos significativos (vermelho)
        pixel = 0;
        pixel = (g << 16) | (r << 8) | b;
        pio_sm_put_blocking(pio, sm, pixel << 8u);
    }

    for (int i = 5; i < 10; i++) // Linha 4
    {
        pixel = _matriz[i];
        b = ((pixel >> 16) & 0xFF) * (_intensidade / 100.00); // Isola os 8 bits mais significativos (azul)
        g = ((pixel >> 8) & 0xFF) * (_intensidade / 100.00);  // Isola os 8 bits intermediários (verde)
        r = (pixel & 0xFF) * (_intensidade / 100.00);         // Isola os 8 bits menos significativos (vermelho)
        pixel = 0;
        pixel = (g << 16) | (r << 8) | b;
        pio_sm_put_blocking(pio, sm, pixel << 8u);
    }

    for (int i = 4; i > -1; i--) // Linha 5
    {
        pixel = _matriz[i];
        b = ((pixel >> 16) & 0xFF) * (_intensidade / 100.00); // Isola os 8 bits mais significativos (azul)
        g = ((pixel >> 8) & 0xFF) * (_intensidade / 100.00);  // Isola os 8 bits intermediários (verde)
        r = (pixel & 0xFF) * (_intensidade / 100.00);         // Isola os 8 bits menos significativos (vermelho)
        pixel = 0;
        pixel = (g << 16) | (r << 8) | b;
        pio_sm_put_blocking(pio, sm, pixel << 8u);
    }
}

void atualizar_display() // Atualizar as informações do display
{
    // PRIORIDADE 1: Alarme disparado (sobrepõe tudo, inclusive modo viagem)
    if (alarme_disparado)
    {
        ssd1306_fill(&ssd, false);                      // limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Moldura
        ssd1306_draw_string(&ssd, "ALERTA", 35, 30);
        ssd1306_send_data(&ssd); // Envia os dados para o display
    }
    // PRIORIDADE 2: Modo viagem (somente aparece se o alarme NÃO estiver disparado)
    else if (modo_viagem)
    {
        ssd1306_fill(&ssd, false);                      // limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Moldura
        ssd1306_draw_string(&ssd, "MODO VIAGEM", 20, 30);
        ssd1306_send_data(&ssd); // Envia os dados para o display
    }
    // PRIORIDADE 3: Estado normal do alarme ON/OFF
    else
    {

        if (alarme)
        {
            ssd1306_fill(&ssd, false);                      // limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Moldura
            ssd1306_draw_string(&ssd, "ALARME ON", 20, 30);
            ssd1306_send_data(&ssd); // Envia os dados para o display
        }
        else
        {
            ssd1306_fill(&ssd, false);                      // limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Moldura
            ssd1306_draw_string(&ssd, "ALARME OFF", 20, 30);
            ssd1306_send_data(&ssd); // Envia os dados para o display
        }
    }
}

void tocar_pwm_buzzer(uint duracao_ms) // Função para Tocar o buzzer
{
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice_num, true);
    sleep_ms(duracao_ms);
    pwm_set_enabled(slice_num, false);
}

void parar_tocar_buzzer()
{
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice_num, false);
}

void monitorar() // FUNÇÃO PARA MONITORAMENTO DOS ALARMES DA CASA
{
    // Dispara o alarme se ainda não foi disparado e condição é verdadeira
    if (!alarme_disparado && ((alarme && porta_open) ||
                              (modo_viagem && (lampada_sala || lampada_quarto || porta_open))))
    {
        alarme_disparado = true;
    }

    // Se o alarme estiver desligado e o modo viagem também, resetar
    if (!alarme && !modo_viagem)
    {
        alarme_disparado = false;
    }

    atualizar_display();

    if (alarme_disparado)
    {
        tocar_pwm_buzzer(800);
    }
    else
    {
        parar_tocar_buzzer();
    }
}

// ========================================================
//            FUNÇÕES DE TRATAMENTO DOS CALLBACKS
// ========================================================

// CONTROLE DA LAMPADA DA SALA
static void controle_lampada_sala(MQTT_CLIENT_DATA_T *state, bool on)
{
    // Publish state on /state topic and on/off led board
    const char *message = on ? "On" : "Off";
    if (on)
    {
        desenha_fig(luz_quarto, BRILHO_PADRAO, pio, sm);
        lampada_quarto = true;
    }
    else
    {
        desenha_fig(matriz_apagada, BRILHO_PADRAO, pio, sm);
        lampada_quarto = false;
    }

    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/lampada_sala/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// CONTROLE DA LAMPADA DO QUARTO
static void controle_lampada_quarto(MQTT_CLIENT_DATA_T *state, bool on)
{
    if (on)
    {
        gpio_put(LED_BLUE_PIN, true);
        gpio_put(LED_GREEN_PIN, true);
        gpio_put(LED_RED_PIN, true);
        lampada_sala = true;
    }
    else
    {
        gpio_put(LED_BLUE_PIN, false);
        gpio_put(LED_GREEN_PIN, false);
        gpio_put(LED_RED_PIN, false);
        lampada_sala = false;
    }
    const char *message = on ? "On" : "Off";
    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/lampada_quarto/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}
// CONTROLE DA PORTA
static void controle_porta(MQTT_CLIENT_DATA_T *state, bool on)
{
    porta_open = !porta_open; // alterna estado
    uint slice_num = pwm_gpio_to_slice_num(SERVO_MOTOR_PIN);
    uint channel = pwm_gpio_to_channel(SERVO_MOTOR_PIN);
    if (on)
    {
        pwm_set_chan_level(slice_num, channel, SERVO_90_DEGREES);
    }
    else
    {
        pwm_set_chan_level(slice_num, channel, SERVO_MIN);
    }
    const char *message = on ? "aberta" : "fechada";
    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/porta/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// CONTROLE DO ALARME
static void controle_alarme(MQTT_CLIENT_DATA_T *state, bool on)
{
    if (on)
    {
        alarme = true;
        atualizar_display();
    }
    else
    {
        alarme = false;
        atualizar_display();
    }
    const char *message = on ? "On" : "Off";
    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/alarme/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}
//  CONTROLE DO MODO VIAGEM
static void controle_modo_viagem(MQTT_CLIENT_DATA_T *state, bool on)
{
    if (on)
    {
        modo_viagem = true;
        lampada_quarto = false;
        lampada_sala = false;
        porta_open = false;
        desenha_fig(matriz_apagada, BRILHO_PADRAO, pio, sm);
        uint slice_num = pwm_gpio_to_slice_num(SERVO_MOTOR_PIN);
        uint channel = pwm_gpio_to_channel(SERVO_MOTOR_PIN);
        pwm_set_chan_level(slice_num, channel, SERVO_MIN);

        // Publicar estados atualizados no modo viagem, permitindo o IoT MQTT PANEL atualizar os switch
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/lampada_quarto/state"), "off", 3, MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/lampada_sala/state"), "off", 3, MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/porta/state"), "fechada", 7, MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);

        gpio_put(LED_BLUE_PIN, false);
        gpio_put(LED_GREEN_PIN, false);
        gpio_put(LED_RED_PIN, false);
    }
    else
    {
        modo_viagem = false;
        atualizar_display();
    }
    const char *message = on ? "On" : "Off";
    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/modo_viagem/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// ========================================================
//            FUNÇÕES DO MQTT_CLIENT_SERVICE
// ========================================================

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
static float read_onboard_temperature(const char unit)
{

    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C' || unit != 'F')
    {
        return tempC;
    }
    else if (unit == 'F')
    {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err)
{
    if (err != 0)
    {
        ERROR_printf("pub_request_cb failed %d", err);
    }
}

// Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name)
{
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

// Publicar temperatura
static void publish_temperature(MQTT_CLIENT_DATA_T *state)
{
    static float old_temperature;
    const char *temperature_key = full_topic(state, "/temperature");
    float temperature = read_onboard_temperature(TEMPERATURE_UNITS);
    if (temperature != old_temperature)
    {
        old_temperature = temperature;
        // Publish temperature on /temperature topic
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);
        INFO_printf("Publishing %s to %s\n", temp_str, temperature_key);
        mqtt_publish(state->mqtt_client_inst, temperature_key, temp_str, strlen(temp_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (err != 0)
    {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (err != 0)
    {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;
    assert(state->subscribe_count >= 0);

    // Stop if requested
    if (state->subscribe_count <= 0 && state->stop_client)
    {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub)
{
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/lampada_sala"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/lampada_quarto"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/porta"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/alarme"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/modo_viagem"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/print"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    DEBUG_printf("basic_topic recebido: '%s'\n", basic_topic);

    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);
    if (strcmp(basic_topic, "/lampada_sala") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            controle_lampada_sala(state, true);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            controle_lampada_sala(state, false);
    }
    else if (strcmp(basic_topic, "/lampada_quarto") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            controle_lampada_quarto(state, true);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            controle_lampada_quarto(state, false);
    }
    else if (strcmp(basic_topic, "/porta") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "aberta") == 0 || strcmp((const char *)state->data, "1") == 0)
            controle_porta(state, true);
        else if (lwip_stricmp((const char *)state->data, "fechada") == 0 || strcmp((const char *)state->data, "0") == 0)
            controle_porta(state, false);
    }
    else if (strcmp(basic_topic, "/alarme") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            controle_alarme(state, true);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            controle_alarme(state, false);
    }
    else if (strcmp(basic_topic, "/modo_viagem") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            controle_modo_viagem(state, true);
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            controle_modo_viagem(state, false);
    }
    else if (strcmp(basic_topic, "/print") == 0)
    {
        INFO_printf("%.*s\n", len, data);
    }
    else if (strcmp(basic_topic, "/ping") == 0)
    {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buf, strlen(buf), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
    else if (strcmp(basic_topic, "/exit") == 0)
    {
        state->stop_client = true;      // stop the client when ALL subscriptions are stopped
        sub_unsub_topics(state, false); // unsubscribe
    }
}

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

// Publicar temperatura
static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)worker->user_data;
    publish_temperature(state);
    async_context_add_at_time_worker_in_ms(context, worker, TEMP_WORKER_TIME_S * 1000);
}

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        state->connect_done = true;
        sub_unsub_topics(state, true); // subscribe;

        // indicate online
        if (state->mqtt_client_info.will_topic)
        {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        // Publish temperature every 10 sec if it's changed
        temperature_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &temperature_worker, 0);
    }
    else if (status == MQTT_CONNECT_DISCONNECTED)
    {
        if (!state->connect_done)
        {
            panic("Failed to connect to mqtt server");
        }
    }
    else
    {
        panic("Unexpected status");
    }
}

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state)
{
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst)
    {
        panic("MQTT client instance creation error");
    }
    INFO_printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    INFO_printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK)
    {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (ipaddr)
    {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    }
    else
    {
        panic("dns request failed");
    }
}