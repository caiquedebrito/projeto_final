#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include <stdio.h>
#include <string.h>

#include "inc/ssd1306.h"

#include "pio_matrix.pio.h"

// ---------------------------------------------------------
// DEFINIÇÕES DE PINOS (ajuste conforme sua BitDogLab)
// ---------------------------------------------------------
#define JOYSTICK_X_ADC_CHANNEL 1   // Ex.: se X estiver em ADC1 (por exemplo, GP27)
#define JOYSTICK_Y_ADC_CHANNEL 0   // Ex.: se Y estiver em ADC0 (por exemplo, GP26)

#define GREEN_LED_PIN    11
#define BLUE_LED_PIN     12
#define RED_LED_PIN      13

// Botão A para desligar/ligar a matriz (por meio de interrupção)
#define BUTTON_A_PIN     5

#define BUZZER_PIN       21
#define BUZZER_FREQUENCY 2000 // Hz

// ---------------------------------------------------------
// DEFINIÇÕES PARA O JOYSTICK / SIMULAÇÃO DOS SENSORES
// ---------------------------------------------------------
#define ADC_MIN_VALUE    20    // Valor mínimo observado
#define ADC_MAX_VALUE    4085  // Valor máximo observado
#define BASELINE_VALUE   2048  // Valor de referência (baseline)

// Direções do joystick para simular cada sensor:
#define THRESHOLD_UP     2500  // Se joy_y > 2500, simula CO₂ (MQ135)
#define THRESHOLD_DOWN   1600  // Se joy_y < 1600, simula CO (MQ7)
#define THRESHOLD_RIGHT  2500  // Se joy_x > 2500, simula Pressão (BMP280)
#define THRESHOLD_LEFT   1600  // Se joy_x < 1600, simula Umidade (DHT22)

// ---------------------------------------------------------
// MODELO ADAPTATIVO E LIMIARES DE ALERTA (em %)
// ---------------------------------------------------------
#define ALPHA            0.2   // Fator para média móvel adaptativa

// Faixas para alertas
#define ALERT_GOOD       60.0
#define ALERT_MODERATE   80.0
#define ALERT_CRITICAL   100.0

// *** Matriz de LED 5x5 controlada via PIO ***
// Número total de LEDs na matriz
#define NUM_PIXELS 25

// Pino de saída para a cadeia de WS2812B (exemplo: use o GPIO definido no seu pio_matrix.pio)
// Neste exemplo, usamos OUT_PIN = 7 para enviar dados à matriz
#define OUT_PIN 7

// ---------------------------------------------------------
// DEFINIÇÕES PARA O DISPLAY OLED SSD1306
// ---------------------------------------------------------
#define I2C_PORT i2c1
#define I2C_ADDR 0x3C
#define I2C_SDA 14
#define I2C_SCL 15

// Variável para controlar o modo crítico (temporizador de 30 s)
volatile bool critical_mode_active = false;
absolute_time_t critical_mode_start;

// Objeto global do display
ssd1306_t display;

// =====================================================
// Padrões para a Matriz de LED 5x5 (usando arrays de 25 doubles)
// Os valores variam de 0.0 (LED apagado) a 1.0 (LED aceso)
double pattern_off[NUM_PIXELS] = {
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0, 
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0
};

void i2c_setup() {
    // Inicialização do I2C em 400 kHz
    i2c_init(I2C_PORT, 400 * 1000);

    // Configuração dos pinos SDA e SCL
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
}

// Função para atualizar o display com o valor do AQI e um gráfico simples
void update_display(float aqi, uint16_t sensor_co2, uint16_t sensor_co, uint16_t sensor_hum, uint16_t sensor_press) {
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    char buffer[17];

    // Linha 0: Exibe o valor do AQI centralizado
    sprintf(buffer, "AQI: %.1f", aqi);
    int len = strlen(buffer);
    int col = (16 - len) / 2;
    ssd1306_draw_string(ssd, col * 8, 0, buffer);

    // Linha 1: Exibe o valor do sensor CO2
    sprintf(buffer, "CO2: %d", sensor_co2);
    ssd1306_draw_string(ssd, 0, 8, buffer);

    // Linha 2: Exibe os valores dos sensores CO e Umidade
    sprintf(buffer, "CO:%d HUM:%d", sensor_co, sensor_hum);
    ssd1306_draw_string(ssd, 0, 16, buffer);

    // Linha 3: Exibe o valor do sensor de Pressao
    sprintf(buffer, "PRES:%d", sensor_press);
    ssd1306_draw_string(ssd, 0, 24, buffer);

    // Linha 4: Gráfico de barras simples representando o AQI
    float norm = (aqi - 50.0f) / 150.0f;
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    int bar_length = (int)(norm * 16);
    char bar[17];
    for (int i = 0; i < 16; i++) {
        bar[i] = (i < bar_length) ? '#' : ' ';
    }
    bar[16] = '\0';
    ssd1306_draw_string(ssd, 0, 32, bar);

    render_on_display(ssd, &frame_area);
}


// ---------------------------------------------------------
// VARIÁVEIS GLOBAIS
// ---------------------------------------------------------
volatile float air_quality_index = 50.0;  // Valor "instantâneo" (opcional)
volatile float aqi_adaptive_avg = 50.0;     // Média móvel adaptativa
volatile bool critical_alert_flag = false;  // Flag para condição crítica

// ---------------------------------------------------------
// FUNÇÕES DO BUZZER (PWM)
// ---------------------------------------------------------
static uint buzzer_slice_num;

void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    buzzer_slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, (float) clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096));
    pwm_init(buzzer_slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);
}

void beep(uint pin, uint duration_ms) {
    pwm_set_gpio_level(pin, 2048);  // Duty cycle ~50%
    sleep_ms(duration_ms);
    pwm_set_gpio_level(pin, 0);
    sleep_ms(100);
}

// ---------------------------------------------------------
// FUNÇÃO PARA REMAPEAR VALORES ADC (normaliza para [0, 1])
// ---------------------------------------------------------
float normalize_adc(uint16_t value) {
    if (value < ADC_MIN_VALUE) value = ADC_MIN_VALUE;
    if (value > ADC_MAX_VALUE) value = ADC_MAX_VALUE;
    return (value - ADC_MIN_VALUE) / (float)(ADC_MAX_VALUE - ADC_MIN_VALUE);
}

// ---------------------------------------------------------
// FUNÇÃO PARA LEITURA DO ADC
// ---------------------------------------------------------
uint16_t read_adc(uint channel) {
    adc_select_input(channel);
    return adc_read();
}

// ---------------------------------------------------------
// FUNÇÃO PARA CALCULAR O AQI COM BASE EM DELTAS
// ---------------------------------------------------------
float calculate_aqi(uint16_t sensor_co2, uint16_t sensor_co, uint16_t sensor_hum, uint16_t sensor_press) {
    float delta_co2 = 0.0f;
    float delta_co = 0.0f;
    float delta_hum = 0.0f;
    float delta_press = 0.0f;

    // Para CO₂ (MQ135): pior qualidade se valor > baseline
    if (sensor_co2 > BASELINE_VALUE) {
        delta_co2 = (sensor_co2 - BASELINE_VALUE) / (float)(ADC_MAX_VALUE - BASELINE_VALUE);
    }
    // Para CO (MQ7): pior qualidade se valor < baseline
    if (sensor_co < BASELINE_VALUE) {
        delta_co = (BASELINE_VALUE - sensor_co) / (float)(BASELINE_VALUE - ADC_MIN_VALUE);
    }
    // Para umidade (DHT22): pior se valor < baseline
    if (sensor_hum < BASELINE_VALUE) {
        delta_hum = (BASELINE_VALUE - sensor_hum) / (float)(BASELINE_VALUE - ADC_MIN_VALUE);
    }
    // Para pressão (BMP280): pior se valor < baseline
    if (sensor_press < BASELINE_VALUE) {
        delta_press = (BASELINE_VALUE - sensor_press) / (float)(BASELINE_VALUE - ADC_MIN_VALUE);
    }
    
    // Calcula a média dos deltas e mapeia para um índice base: 50 (baseline) até 200 (pior)
    float avg_delta = (delta_co2 + delta_co + delta_hum + delta_press) / 4.0f;
    return 50.0f + avg_delta * 150.0f;
}

// ---------------------------------------------------------
// TIMER CALLBACK: Atualiza alertas (LEDs) com base no AQI adaptativo
// ---------------------------------------------------------
bool timer_callback(struct repeating_timer *t) {
    if (aqi_adaptive_avg < ALERT_GOOD) {
        gpio_put(GREEN_LED_PIN, 1);
        gpio_put(BLUE_LED_PIN, 0);
        gpio_put(RED_LED_PIN, 0);
        critical_alert_flag = false;
    } else if (aqi_adaptive_avg < ALERT_MODERATE) {
        gpio_put(GREEN_LED_PIN, 1);
        gpio_put(BLUE_LED_PIN, 0);
        gpio_put(RED_LED_PIN, 1);
        critical_alert_flag = false;
    } else if (aqi_adaptive_avg < ALERT_CRITICAL) {
        gpio_put(GREEN_LED_PIN, 0);
        gpio_put(BLUE_LED_PIN, 0);
        gpio_put(RED_LED_PIN, 1);
        critical_alert_flag = false;
    } else {
        static bool blink = false;
        blink = !blink;
        gpio_put(GREEN_LED_PIN, 0);
        gpio_put(BLUE_LED_PIN, 0);
        gpio_put(RED_LED_PIN, blink);
        critical_alert_flag = true;
    }
    return true;
}

// =====================================================
// PIO – Controle da Matriz de LED 5x5
// =====================================================
// O programa PIO definido em "pio_matrix.pio" controla a matriz WS2812B
// (ou similar) utilizando side‑set para selecionar as linhas.
// O arquivo "pio_matrix.pio.h" deve conter o pio_program_t led_matrix_program
// e a função de inicialização pio_matrix_program_init().

PIO pio_matrix = pio0;
uint sm_matrix;

void init_led_matrix() {
    uint offset = pio_add_program(pio_matrix, &pio_matrix_program);
    sm_matrix = pio_claim_unused_sm(pio_matrix, true);
    // Inicializa o estado‑máquina PIO: utiliza OUT_PIN (definido acima) e configura a saída para NUM_PIXELS
    pio_matrix_program_init(pio_matrix, sm_matrix, offset, OUT_PIN);
}

// Função para converter valores (0.0 a 1.0) em uma cor 32-bit (RGB)
uint32_t matrix_rgb(double intensity) {
    // Converte o valor de intensidade para 0-255
    unsigned char I = intensity * 255;
    // Retorna a cor no formato 0x00RRGGBB (usaremos apenas R, por exemplo)
    return (I << 16) | (I << 8) | I;
}

// Função para atualizar a matriz de LED 5x5 via PIO
// O vetor 'pattern' tem 25 elementos (um para cada LED)
void update_led_matrix(double *pattern) {
    // Para cada LED (índice de 0 a 24), envia o valor correspondente
    for (int i = 0; i < NUM_PIXELS; i++) {
        uint32_t valor_led = matrix_rgb(pattern[24 - i]);
        pio_sm_put_blocking(pio_matrix, sm_matrix, valor_led);
    }
}

// ================================
// FUNÇÃO PARA VERIFICAR A CONDIÇÃO CRÍTICA (30 s)
// ================================
void check_critical_mode() {
    if (!critical_mode_active && aqi_adaptive_avg >= ALERT_CRITICAL) {
        critical_mode_active = true;
        critical_mode_start = get_absolute_time();
    }
    if (critical_mode_active) {
        // Aciona o buzzer periodicamente (exemplo: a cada 1 segundo)
        beep(BUZZER_PIN, 100);
        // Verifica se 30 segundos se passaram
        uint32_t diff_sec = absolute_time_diff_us(critical_mode_start, get_absolute_time()) / 1000000;
        if (diff_sec >= 30) {
            // Reinicia o AQI para o valor inicial, simulando a atuação dos atuadores
            aqi_adaptive_avg = 50.0;
            critical_mode_active = false;
            printf("30 segundos críticos completos. Reiniciando AQI.\n");
        }
    }
}

// ================================
// FUNÇÃO PARA ATUADORES: SIMULA VENTILADOR E VÁLVULA
// ================================
// Utiliza dois LEDs da matriz (por exemplo, índice 5 e 19) para mostrar a intensidade
// dos atuadores. Se o AQI estiver em nível crítico, a função reduz o valor de aqi_adaptive_avg
// para simular que os atuadores estão atuando.
void update_actuators() {
    // Define intensidade dos atuadores proporcional ao AQI adaptativo, limitado a [0,1]
    float actuator_level = 0.0f;
    if (aqi_adaptive_avg >= ALERT_CRITICAL) {
        actuator_level = 1.0f;
    } else {
        // Proporcional: se aqi está entre ALERT_MODERATE e ALERT_CRITICAL
        actuator_level = (aqi_adaptive_avg - ALERT_MODERATE) / (ALERT_CRITICAL - ALERT_MODERATE);
        if (actuator_level < 0.0f) actuator_level = 0.0f;
        if (actuator_level > 1.0f) actuator_level = 1.0f;
    }
    
    double pattern_actuators[NUM_PIXELS];
    for (int i = 0; i < NUM_PIXELS; i++) {
        pattern_actuators[i] = 0.0;
    }
    // Define os LEDs para o ventilador (posição 5) e para a válvula (posição 19)
    pattern_actuators[14] = actuator_level;
    pattern_actuators[10] = actuator_level;
    
    // Atualiza a matriz com esses dois LEDs ativos
    update_led_matrix(pattern_actuators);
}

// =====================================================
// FUNÇÃO DE INTERRUPÇÃO PARA O BOTÃO A (desliga/liga a matriz)
// =====================================================
volatile bool button_pressed = false;

// =====================================================
// Variável global para controle da matriz (habilitada ou não)
volatile bool matrix_enabled = true;

void button_a_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_A_PIN) {
        // Alterna o estado da matriz
        matrix_enabled = !matrix_enabled;
        if (!matrix_enabled) {
            // Desliga a matriz enviando o padrão "off"
            update_led_matrix(pattern_off);
        }
    }
}

int main() {
    stdio_init_all();
    adc_init();

    // Inicializa os pinos do joystick (ex.: GP26 e GP27)
    adc_gpio_init(26);
    adc_gpio_init(27);

    // Inicializa os LEDs de alerta
    gpio_init(GREEN_LED_PIN);  gpio_set_dir(GREEN_LED_PIN, GPIO_OUT);
    gpio_init(BLUE_LED_PIN); gpio_set_dir(BLUE_LED_PIN, GPIO_OUT);
    gpio_init(RED_LED_PIN);    gpio_set_dir(RED_LED_PIN, GPIO_OUT);

    // Inicializa o buzzer
    pwm_init_buzzer(BUZZER_PIN);

    init_led_matrix();

    // Configura o botão A (pino 5) para interrupção
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, button_a_callback);

    // Inicializa o display SSD1306 via I2C
    i2c_setup();

    // Cria um timer repetidor (500 ms) para atualizar os alertas
    struct repeating_timer timer;
    add_repeating_timer_ms(500, timer_callback, NULL, &timer);

    printf("Sistema Integrado: Monitoramento Multivariável e Alertas Inteligentes\n");

    while (true) {
        // Leitura do joystick
        uint16_t joy_x = read_adc(JOYSTICK_X_ADC_CHANNEL);
        uint16_t joy_y = read_adc(JOYSTICK_Y_ADC_CHANNEL);

        // Simula os sensores:
        // Se o joystick for movido para cima (Y > THRESHOLD_UP), usa o valor; caso contrário, baseline.
        uint16_t sensor_co2   = (joy_y > THRESHOLD_UP)    ? joy_y : BASELINE_VALUE;
        // Se movido para baixo (Y < THRESHOLD_DOWN), simula CO; caso contrário, baseline.
        uint16_t sensor_co    = (joy_y < THRESHOLD_DOWN)  ? joy_y : BASELINE_VALUE;
        // Se movido para a esquerda (X < THRESHOLD_LEFT), simula umidade; caso contrário, baseline.
        uint16_t sensor_hum   = (joy_x < THRESHOLD_LEFT)  ? joy_x : BASELINE_VALUE;
        // Se movido para a direita (X > THRESHOLD_RIGHT), simula pressão; caso contrário, baseline.
        uint16_t sensor_press = (joy_x > THRESHOLD_RIGHT) ? joy_x : BASELINE_VALUE;

        // Calcula o índice de qualidade com a nova fórmula baseada em deltas
        float aqi_current = calculate_aqi(sensor_co2, sensor_co, sensor_hum, sensor_press);

        // Atualiza a média móvel adaptativa
        aqi_adaptive_avg = ALPHA * aqi_current + (1.0f - ALPHA) * aqi_adaptive_avg;

        // Exibe os valores para depuração
        printf("Joystick X: %d, Y: %d\n", joy_x, joy_y);
        printf("CO2: %d, CO: %d, HUM: %d, PRESS: %d\n",
               sensor_co2, sensor_co, sensor_hum, sensor_press);
        printf("AQI Atual: %.2f, Média Adaptativa: %.2f\n", aqi_current, aqi_adaptive_avg);
        printf("------------------------------------------\n");

        // Se o nível adaptativo estiver crítico, emite beep
        if (critical_alert_flag) {
            beep(BUZZER_PIN, 100);
        }

        // --- Gerencia o modo crítico (30 segundos) ---
        check_critical_mode();

        // --- Parte 3: Atualiza a matriz de LED via PIO (se habilitada) ---
        if (matrix_enabled) {
            update_actuators();
        }

        // --- Parte 4: Atualiza a interface visual no Display ---
        update_display(aqi_adaptive_avg, sensor_co2, sensor_co, sensor_hum, sensor_press);

        sleep_ms(1000);
    }

    return 0;
}
