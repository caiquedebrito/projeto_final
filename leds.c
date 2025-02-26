#include "pico/stdlib.h"

// Definições dos pinos para os alertas
#define GREEN_LED_PIN    11    // LED verde: indica qualidade boa
#define YELLOW_LED_PIN   12    // LED amarelo: indica qualidade moderada
#define RED_LED_PIN      13    // LED vermelho: indica qualidade ruim ou crítica

void leds_setup() {
  // Inicializa os pinos dos LEDs e buzzer como saída
  gpio_init(GREEN_LED_PIN);
  gpio_set_dir(GREEN_LED_PIN, GPIO_OUT);
  gpio_init(YELLOW_LED_PIN);
  gpio_set_dir(YELLOW_LED_PIN, GPIO_OUT);
  gpio_init(RED_LED_PIN);
  gpio_set_dir(RED_LED_PIN, GPIO_OUT);
}