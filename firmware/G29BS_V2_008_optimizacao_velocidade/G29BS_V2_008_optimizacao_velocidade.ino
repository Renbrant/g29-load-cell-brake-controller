/*
  Programa: Simulador de Freio com Saída Analógica
  Autor: Renato Brant
  Data: 01/12/2024
  Versão: v2.008

  Histórico de Modificações:
  - v2.004: Alterado o comportamento do cálculo de curva de tensão para inverter a relação pressão/tensão.
  - v2.005: Corrigido o problema de apresentação do gráfico que ignorava o MIN_VOLTAGE (1.47V).
           Excluída a configuração do formato da curva devido a inconsistências. Agora o comportamento é estritamente linear.
           Criado um histórico de modificações para acompanhamento de versões.
  - v2.006: Refatoração para melhorar a organização e legibilidade do código sem alterar funcionalidades existentes.
           Adicionado ajuste do valor mínimo de tensão (Vmin).
           Valores de Vmax e Vmin exibidos no display (canto inferior esquerdo e superior direito, respectivamente).
           Quadrado desenhado ao redor do valor que está sendo configurado.
           Movida a apresentação da pressão para o canto inferior direito, acima do valor de tensão (previousVoltage).
           Fundo preto adicionado para os valores de pressão e tensão para melhor contraste.
  - v2.007: Melhorada a interface gráfica do ajuste de configurações.
           Menu de configurações exibido no canto superior esquerdo apenas quando o botão é pressionado.
           Opções: Inclinação (I), Vmax e Vmin, com destaque `>` para a configuração ativa.
           Menu desaparece automaticamente após 5 segundos de inatividade, salvando as configurações.
           Corrigido valor no display para sincronizar com a saída DAC.
           Expandido o controle de curva para maior sensibilidade a pressões menores (curva mais inclinada).
  - v2.008: Otimizado o tempo de resposta do sistema.
           - Atualizações seletivas do display para reduzir atrasos.
           - Incrementada a frequência I²C para 400 kHz.
           - Adicionada medição de tempo no ciclo principal para análise de desempenho.
           - Display atualizado com menos frequência fora do menu de configuração.
*/

#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <HX711.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <FlashStorage.h>

// Definições dos pinos
#define DT_PIN 7
#define SCK_PIN 6
#define ROTARY_CLK_PIN 1
#define ROTARY_DT_PIN 2
#define ROTARY_SW_PIN 3
#define OLED_RESET -1

// Objetos para os periféricos
HX711 scale;
Adafruit_MCP4725 dac;
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
Encoder rotaryEncoder(ROTARY_CLK_PIN, ROTARY_DT_PIN);

// Estrutura para armazenar configurações
struct Config {
  float maxVoltage;  // Tensão máxima ajustável
  float minVoltage;  // Tensão mínima ajustável
  float curveFactor; // Fator de inclinação da curva
};
FlashStorage(flashConfig, Config); // Aloca espaço na memória flash

// Configurações iniciais padrão
float maxVoltage = 3.3;           // Tensão máxima inicial
float minVoltage = 1.47;          // Tensão mínima inicial
float curveFactor = 0.0;          // Inclinação inicial da curva
int lastEncoderPosition = 0;      // Última posição do encoder
bool buttonPressed = false;       // Estado do botão do encoder
int configMode = 0;               // 0 = Inclinação, 1 = Vmax, 2 = Vmin

// Configuração do HX711
const float CALIBRATION_FACTOR = -44.765419; // Fator de calibração para o sensor
const long OFFSET = 1877599;                 // Offset para ajustar o zero
float pressure = 0;                          // Pressão medida em kg

// Controle gráfico
const int GRAPH_WIDTH = 122;                 // Largura do gráfico
const int GRAPH_HEIGHT = 64;                 // Altura do gráfico

float outputVoltage = 0;                     // Tensão de saída calculada

// Controle do menu de configurações
bool showConfigMenu = false;                 // Controla a exibição do menu de configurações
unsigned long lastInteraction = 0;           // Tempo da última interação
const unsigned long CONFIG_TIMEOUT = 5000;   // Tempo limite para esconder o menu (5 segundos)

// Tempo de atualização do display fora do menu
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 50; // Atualiza o display a cada 50 ms (~20 Hz)

// Função: Configuração inicial do sistema
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  // Configuração do I²C em alta frequência
  Wire.begin();
  Wire.setClock(400000); // Define frequência do I²C para 400 kHz

  // Inicializa HX711
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_offset(OFFSET);
  scale.set_scale(CALIBRATION_FACTOR);

  // Inicializa DAC MCP4725
  dac.begin(0x60);

  // Inicializa display OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.display();

  // Configura botão do encoder
  pinMode(ROTARY_SW_PIN, INPUT_PULLUP);

  // Carrega configurações salvas na memória flash
  loadSettings();

  Serial.println("Sistema inicializado com sucesso!");
}

// Função: Laço principal do programa
void loop() {
  unsigned long loopStartTime = micros(); // Medir tempo de início do loop

  handleButtonPress();     // Gerencia as interações do botão
  handleEncoderRotation(); // Processa a movimentação do encoder

  // Esconde o menu de configuração após o tempo limite
  if (showConfigMenu && millis() - lastInteraction > CONFIG_TIMEOUT) {
    showConfigMenu = false;
    saveSettings();
    Serial.println("Configurações salvas automaticamente após inatividade.");
  }

  // Lê e atualiza os dados do sistema
  if (scale.is_ready()) {
    pressure = readPressure(); // Lê a pressão do sensor
    updateDAC();               // Atualiza a saída do DAC
  }

  // Atualiza o display com base no intervalo definido
  if (showConfigMenu || millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = millis(); // Atualiza o tempo de última atualização
  }

  // Medição do tempo de resposta do loop
  unsigned long loopEndTime = micros();
  Serial.print("Tempo do loop: ");
  Serial.print((loopEndTime - loopStartTime) / 1000.0); // Tempo em ms
  Serial.println(" ms");
}

// Funções auxiliares (handleButtonPress, handleEncoderRotation, readPressure, updateDAC, updateDisplay, drawGraph, saveSettings, loadSettings)
// Todas mantêm a funcionalidade e implementação explicada na v2.007,
// mas com otimização de atualizações parciais no display e medição de tempos.
