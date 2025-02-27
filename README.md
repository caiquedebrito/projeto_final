# Monitor de Qualidade do Ar para Quartos de Maternidade

Este é o repositório do projeto final do programa de capacitação em sistemas embarcados – **Embarcatech**. O projeto consiste em um simulador de monitoramento da qualidade do ar, com foco em quartos de maternidade, integrando sensores simulados, alertas inteligentes, atuação corretiva (ventilador e válvula proporcional) e uma interface visual composta por um display OLED SSD1306 e uma matriz de LEDs.

---

## Sumário

- [Descrição do Projeto](#descrição-do-projeto)
- [Funcionalidades](#funcionalidades)
- [Arquitetura de Hardware](#arquitetura-de-hardware)
- [Especificação do Firmware](#especificação-do-firmware)
- [Como Executar o Projeto](#como-executar-o-projeto)
- [Documentação Complementar](#documentação-complementar)
- [Referências](#referências)

---

## Descrição do Projeto

O **Monitor de Qualidade do Ar para Quartos de Maternidade** é um sistema embarcado inovador que simula o monitoramento ambiental, avaliando parâmetros como CO₂, monóxido de carbono, umidade e pressão atmosférica. O sistema utiliza:

- **Sensores simulados** (via joystick/potenciômetros) para captar os valores ambientais.
- **Modelo adaptativo** para calcular um Índice de Qualidade do Ar (AQI).
- **Alertas inteligentes**: LEDs de alerta (verde, amarelo e vermelho) e um buzzer que informam o usuário sobre o estado da qualidade do ar.
- **Atuadores simulados**: Dois LEDs da matriz de LED 5×5 (controlados via PIO) que simulam um ventilador e uma válvula proporcional. Quando o AQI atinge níveis críticos, os atuadores operam em intensidade máxima por 30 segundos e, ao final, o índice adaptativo é reinicializado.
- **Interface visual**: Um display OLED SSD1306 exibe informações detalhadas, incluindo o valor do AQI, as leituras dos sensores e gráficos simples.

---

## Funcionalidades

1. **Monitoramento Multivariável Simulado**  
   - Leitura dos valores analógicos do joystick para simular sensores de CO₂, CO, umidade e pressão.
   - Cálculo do AQI com base em uma média móvel adaptativa.

2. **Alertas Inteligentes**  
   - Indicadores visuais com LEDs: verde (bom), amarelo (moderado) e vermelho (crítico).
   - Alerta sonoro via buzzer, acionado quando o AQI atinge níveis críticos.

3. **Atuação Corretiva**  
   - Dois LEDs da matriz de LED 5×5 (em posições definidas) simulam a atuação do ventilador e da válvula proporcional.
   - Quando o AQI atinge o nível crítico, os atuadores operam em intensidade máxima durante 30 segundos e, em seguida, o AQI adaptativo é reinicializado para indicar a ação corretiva.

4. **Interface Visual**  
   - Exibição no display OLED SSD1306 do valor do AQI centralizado (por exemplo, “AQI: 43.1”).
   - Apresentação de valores individuais dos sensores e um gráfico de barras simples que ilustra a variação do AQI.

---

## Arquitetura de Hardware

### Diagrama em Bloco (Resumo)
![Image](https://github.com/user-attachments/assets/0b62fab6-743d-4ceb-9313-e1c96ea97497)

### Circuito Completo

- **Fonte de Alimentação:**  
  - Terminais VCC e GND do RP2040 conectados a uma fonte regulada de 3,3V.

- **Microcontrolador (RP2040):**  
  - **ADC:**  
    - GP26 (ADC0): Conectado aos potenciômetros do joystick (eixo Y).
    - GP27 (ADC1): Conectado aos potenciômetros do joystick (eixo X).
  - **PWM:**  
    - GP21 (exemplo): Usado para o buzzer.
  - **PIO:**  
    - GP7: Configurado como saída para enviar dados à matriz de LED (controlada via PIO).
  - **Comunicação I²C:**  
    - GP14 (SDA) e GP15 (SCL): Conectados ao display OLED SSD1306.
  - **LEDs de Alerta:**  
    - GP11, GP12 e GP13: Usados para os LEDs verde, amarelo e vermelho, respectivamente.
  - **Botão de Interrupção:**  
    - GP5: Usado para o botão A (com resistor pull‑up).

- **Módulo de Sensores Simulados:**  
  - Joystick/Potenciômetros conectados aos pinos GP26 e GP27.

- **Display SSD1306:**  
  - Conectado via I²C (GP14 e GP15) com endereço 0x3C.

- **Matriz de LED 5×5:**  
  - Controlada pelo pino GP7 via PIO.

---

## Especificação do Firmware

### Blocos Funcionais – Diagrama de Camadas do Software

![Image](https://github.com/user-attachments/assets/37f6def4-f728-4206-ad97-9b6908e460b6)

### Descrição das Funcionalidades dos Blocos

- **Drivers de Hardware:**  
  - Inicializam e configuram os módulos periféricos: ADC (para o joystick), PWM (para o buzzer), PIO (para a matriz de LED) e I²C (para o display).  
  - Fornecem funções de baixo nível para leitura e escrita em registradores.

- **Lógica de Controle:**  
  - Lê os valores dos sensores simulados, normaliza os dados e calcula o AQI usando uma média móvel adaptativa.
  - Verifica thresholds para determinar os estados (bom, moderado, crítico) e gerencia um temporizador de 30 segundos para o modo crítico.  
  - Quando o AQI atinge o nível crítico, aciona os atuadores (ventilador e válvula simulados) em intensidade máxima e, ao final dos 30 segundos, reinicializa o AQI adaptativo.

- **Gerenciamento de Comunicação:**  
  - Coordena a comunicação com o display via I²C, enviando comandos e dados para atualização da interface.  
  - Controla a matriz de LED por meio do PIO, enviando os dados dos atuadores.

- **Aplicação/Interface (UI):**  
  - Atualiza o display para mostrar o valor do AQI, os valores dos sensores e um gráfico de barras simples.  
  - Fornece feedback visual ao usuário, centralizando as informações no display.

### Definição das Variáveis Principais

- **Sensores e AQI:**  
  - `uint16_t sensor_co2, sensor_co, sensor_hum, sensor_press;` – Valores brutos dos sensores simulados.
  - `float aqi_current;` – Valor instantâneo do AQI.
  - `float aqi_adaptive_avg;` – Média móvel adaptativa do AQI.
  - `volatile float air_quality_index;` – Valor atual do AQI para debug.

- **Alertas e Temporização Crítica:**  
  - `volatile bool critical_alert_flag;` – Indica condição crítica.
  - `volatile bool critical_mode_active;` – Indica se o temporizador de 30 segundos está ativo.
  - `absolute_time_t critical_mode_start;` – Tempo de início do modo crítico.

- **Atuadores (Matriz de LED):**  
  - Array de 25 elementos para representar os LEDs da matriz, com índices específicos para o ventilador e a válvula (ex.: índices 5 e 19).

- **Display:**  
  - Buffer de renderização (`uint8_t ssd[ssd1306_buffer_length]`).

### Fluxograma Completo do Software

![Image](https://github.com/user-attachments/assets/b6c2cba7-659b-419f-9f89-0319a8d0abf4)

### Processo de Inicialização

Durante a inicialização, o firmware:
- Chama `stdio_init_all()` para configurar a comunicação serial (depuração via UART).
- Inicializa o ADC com `adc_init()` e configura os pinos do joystick (GP26 e GP27) com `adc_gpio_init()`.
- Configura os pinos GPIO para os LEDs de alerta e para o botão (GP5) com resistores pull‑up.
- Inicializa o PWM para o buzzer por meio de `pwm_init_buzzer()`, definindo a frequência (2000 Hz) e o duty cycle inicial.
- Carrega o programa PIO para a matriz de LED 5×5, alocando um estado‑máquina com `pio_add_program()`, `pio_claim_unused_sm()` e `pio_matrix_program_init()`.
- Inicializa o barramento I²C (por exemplo, usando i2c1) e configura os pinos SDA (GP14) e SCL (GP15) para comunicação com o display SSD1306. Em seguida, o display é configurado com `ssd1306_init()` e `ssd1306_config()`.
- Configura um temporizador repetidor com `add_repeating_timer_ms(500, timer_callback, ...)` para atualizar os alertas.
- Após a configuração de todos os módulos, o firmware entra no loop principal, onde continuamente lê os sensores, processa os dados para calcular o AQI e atualiza a interface visual e os atuadores.

### Configuração dos Registros

- **ADC:**  
  - `adc_init()` habilita e configura os registradores do ADC.
  - `adc_gpio_init(pin)` configura os pinos para função ADC.
  - `adc_select_input(channel)` define o canal ativo para leitura.

- **PWM:**  
  - `pwm_get_default_config()` obtém a configuração padrão.
  - `pwm_config_set_clkdiv()` ajusta o divisor de clock para atingir a frequência desejada.
  - `pwm_init()` configura os registradores de controle e habilita o PWM.
  - `pwm_set_gpio_level()` define o duty cycle.

- **PIO:**  
  - `pio_add_program()` carrega o código do programa PIO nos registradores de instrução.
  - `pio_claim_unused_sm()` aloca um estado‑máquina disponível.
  - `pio_sm_init()` configura os registradores do SM (pinos de saída, side‑set, clock divisor).

- **I²C:**  
  - `i2c_init()` configura os registradores do I²C, definindo a frequência e os divisores de clock.
  - `gpio_set_function()` e `gpio_pull_up()` configuram os pinos SDA e SCL para a função I²C.

- **GPIO:**  
  - `gpio_init()`, `gpio_set_dir()` e `gpio_pull_up()` configuram os pinos para entrada ou saída e definem resistores internos, conforme necessário.

### Estrutura e Formato dos Dados

- **Sensores (ADC):**  
  - Valores lidos (uint16_t) normalizados para um intervalo de 0 a 1.
  
- **AQI:**  
  - Calculado como float com base na média móvel adaptativa, variando aproximadamente de 50 a 200.
  
- **Buffer do Display:**  
  - Array de uint8_t com tamanho definido por `ssd1306_buffer_length`, organizado em páginas (cada byte representa 8 pixels na vertical).

- **Matriz de LED (PIO):**  
  - Dados são organizados em arrays de 25 valores (double, de 0.0 a 1.0), convertidos em uint32_t (via matrix_rgb) para definir a intensidade dos LEDs.
  
- **Temporizadores e Flags:**  
  - Uso de estruturas absolute_time_t para gerenciamento de tempo e variáveis booleanas para controle de estados críticos.

### Protocolo de Comunicação e Formato dos Pacotes

- **Display (I²C):**  
  - O display SSD1306 utiliza o protocolo I²C com endereço 0x3C.  
  - Os pacotes são compostos por um byte de controle (0x00 para comandos, 0x40 para dados) seguido de uma sequência de bytes que representam os pixels do display, organizados em páginas.

- **Matriz de LED (PIO):**  
  - Cada LED é controlado por um pacote de 32 bits (uint32_t) que codifica a intensidade (em escala de cinza) do LED, conforme gerado pela função matrix_rgb().  
  - Esses valores são enviados via FIFO do estado‑máquina PIO com a função `pio_sm_put_blocking()`.

---

## Execução do Projeto

### Metodologia

1. **Pesquisa e Levantamento de Requisitos:**  
   - Pesquisa sobre monitoramento da qualidade do ar com sistemas embarcados e revisão de projetos similares, com foco em ambientes sensíveis (quartos de maternidade).

2. **Escolha do Hardware:**  
   - Seleção do microcontrolador RP2040 (KitBitDogLab), display SSD1306, joystick (para simulação dos sensores), LEDs para alertas, buzzer e matriz de LED 5×5 para simular atuadores.

3. **Desenvolvimento do Firmware:**  
   - Estruturação do firmware em camadas funcionais: drivers de hardware, lógica de controle (leitura dos sensores, cálculo do AQI, gerenciamento de alertas e temporizador crítico), gerenciamento de comunicação e interface do usuário.
   - Utilização do Visual Studio Code com o Pico SDK para programação e depuração, com uso de saída serial para monitoramento dos dados.

4. **Integração e Testes:**  
   - Integração das funcionalidades (monitoramento, alertas, atuadores e interface visual) e realização de testes individuais e integrados para validar a estabilidade do sistema.

### Testes de Validação

- **Sensores Simulados:**  
  - Verificação da variação dos valores lidos via ADC conforme o movimento do joystick.
- **Alertas e Atuadores:**  
  - Testes dos LEDs de alerta e do buzzer, garantindo acionamento correto conforme os thresholds do AQI.
  - Verificação do temporizador de 30 segundos para o modo crítico, acionando os atuadores (LEDs na matriz) e reinicializando o AQI adaptativo.
- **Interface Visual:**  
  - Validação da exibição no display OLED dos valores do AQI, dos sensores e do gráfico de barras.
- **Testes Integrados:**  
  - Operação contínua do sistema, demonstrando a comunicação estável entre os módulos e a confiabilidade do firmware.

### Discussão dos Resultados

- **Comportamento do Sistema:**  
  - O sistema responde adequadamente às variações dos sensores simulados, acionando alertas visuais e sonoros conforme os níveis do AQI.
  - Quando o nível crítico é atingido, o temporizador de 30 segundos inicia, os atuadores simulados operam em intensidade máxima e o AQI adaptativo é reinicializado, demonstrando a atuação corretiva.
- **Confiabilidade:**  
  - Os testes integrados e de estresse indicaram que o firmware é robusto, com comunicação confiável entre os módulos.
- **Aplicabilidade:**  
  - O sistema é de baixo custo, compacto e pode ser adaptado para ambientes reais com sensores específicos, sendo ideal para monitorar a qualidade do ar em quartos de maternidade.

---

## Como Executar o Projeto

1. **Pré-Requisitos:**  
   - KitBitDogLab com microcontrolador RP2040.
   - Conexões de hardware conforme o circuito completo.
   - Ferramentas: Visual Studio Code com Pico SDK, compilador GCC para ARM.

2. **Clonar o Repositório:**

   ````
   git clone https://github.com/caiquedebrito/projeto_final.git
   cd projeto_final
   ````

3. **Configurar o Ambiente**:
 - Certifique-se de que o Pico SDK esteja configurado e que as variáveis de ambiente estejam definidas conforme a documentação do Pico.

4. **Compilar o Projeto**

  ```
  mkdir build && cd build
  cmake ..
  make
  ```

5. **Carregar o Firmware**:
  - Conecte o KitBitDogLab e coloque-o no modo BOOTSEL.
  - Copie o arquivo UF2 gerado para o dispositivo montado como unidade USB.

6. **Executar e Monitorar**:
   - Após a carga do firmware, o sistema iniciará automaticamente.
   - Observe os alertas visuais (LEDs), o buzzer e a atualização do display OLED.
   - Quando o AQI atingir o nível crítico, o temporizador de 30 segundos iniciará, os atuadores (dois LEDs da matriz) operarão em intensidade máxima e, após o período, o AQI será reinicializado para o valor inicial.
  
## Documentação Complementar
- Documentação do projeto: [Clique aqui para acessar a documentação completa](https://docs.google.com/document/d/1S3_TE4NdNCpEue2aa09VA30dvPFJ8IV_fP4KXtUcpow/edit?usp=sharing).
- Vídeo de apresentação: [Assista ao vídeo de apresentação do projeto](https://youtu.be/PVpRytIcTRc).

## Referências

1. SENTHILKUMAR, R.; VENKATAKRISHNAN, P.; BALAJI, N. Intelligent based novel embedded system based IoT enabled air pollution monitoring system. Microprocessors and Microsystems, v. 77, p. 103172, 2020.
2. TIONG, Ngiam Kee. Constructing an air quality detection system using embedded system. Trabalho de Conclusão de Curso (Bacharelado em Tecnologia com Honras em Sistemas Eletrônicos) – Faculty of Engineering and Green Technology, Universiti Tunku Abdul Rahman, Malásia, 2023.
3. RASPBERRY PI. Raspberry Pi Pico Datasheet. Disponível em: https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf. Acesso em: 21 fev. 2025.
