/*
  Programa: Simulador de Freio com Saída Analógica
  Autor: Renato Brant
  Data: 01/12/2024
  Versão: v2.007

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

// Função: Configuração inicial do sistema
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

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
    updateDisplay();           // Atualiza a tela OLED
  }
}

// Função: Lê o estado do botão do encoder
void handleButtonPress() {
  bool currentButtonState = digitalRead(ROTARY_SW_PIN) == LOW;
  if (currentButtonState && !buttonPressed) {
    if (!showConfigMenu) {
      showConfigMenu = true; // Ativa o menu de configurações
    } else {
      configMode = (configMode + 1) % 3; // Alterna entre Inclinação, Vmax e Vmin
    }
    lastInteraction = millis(); // Atualiza o tempo de interação
  }
  buttonPressed = currentButtonState;
}

// Função: Lê o movimento do encoder e ajusta as configurações
void handleEncoderRotation() {
  if (showConfigMenu) { // Permite ajustes apenas quando o menu está ativo
    int newEncoderPosition = rotaryEncoder.read() / 2; // Divide por 2 para maior controle
    if (newEncoderPosition != lastEncoderPosition) {
      int delta = newEncoderPosition - lastEncoderPosition;

      if (configMode == 0) { // Ajusta a Inclinação
        curveFactor += delta * 0.05;
        curveFactor = constrain(curveFactor, -2.0, 2.0); // Limites expandidos
      } else if (configMode == 1) { // Ajusta o Vmax
        maxVoltage += delta * 0.05;
        maxVoltage = constrain(maxVoltage, 2.5, 3.3);
      } else if (configMode == 2) { // Ajusta o Vmin
        minVoltage += delta * 0.05;
        minVoltage = constrain(minVoltage, 1.0, maxVoltage - 0.1);
      }

      lastEncoderPosition = newEncoderPosition;
      lastInteraction = millis(); // Atualiza o tempo de interação
    }
  }
}

// Função: Lê a pressão do sensor HX711
float readPressure() {
  float measuredPressure = scale.get_units() / 1000.0; // Converte para kg
  return constrain(measuredPressure, 0.0, 50.0); // Limita a pressão entre 0 e 50 kg
}

// Função: Calcula e aplica a tensão ao DAC
void updateDAC() {
  float normalizedPressure = pressure / 50.0; // Normaliza a pressão (0 a 1)
  normalizedPressure = constrain(normalizedPressure, 0.0, 1.0);

  float adjustedPressure = normalizedPressure * (1.0 + curveFactor);
  adjustedPressure = constrain(adjustedPressure, 0.0, 1.0);

  // Calcula a tensão de saída
  outputVoltage = minVoltage + (1.0 - adjustedPressure) * (maxVoltage - minVoltage);

  // Converte para 12 bits e atualiza o DAC
  uint16_t dacOutput = (uint16_t)((outputVoltage / 3.3) * 4095);
  dac.setVoltage(dacOutput, false);
}

// Função: Atualiza o display OLED com os dados mais recentes
void updateDisplay() {
  display.clearDisplay();
  drawGraph();

  if (showConfigMenu) { // Exibe o menu de configurações, se ativo
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setTextColor(WHITE);
    display.print(configMode == 0 ? "> I: " : "  I: ");
    display.println(curveFactor, 2);
    display.print(configMode == 1 ? "> Vmax: " : "  Vmax: ");
    display.println(maxVoltage, 2);
    display.print(configMode == 2 ? "> Vmin: " : "  Vmin: ");
    display.println(minVoltage, 2);
  } else { // Exibe os valores principais
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.print(String(pressure, 0));
    display.setTextSize(1);
    display.print("kg");

    display.fillRect(66, 48, 58, 16, BLACK);
    display.setCursor(66, 48);
    display.setTextSize(2);
    display.print(String(outputVoltage, 2));
    display.setTextSize(1);
    display.print("V");
  }

  display.display();
}

// Função: Desenha o gráfico de curva no display
void drawGraph() {
  for (int x = 0; x < GRAPH_WIDTH; x++) {
    float rawPos = (float)x / GRAPH_WIDTH;
    float curveY = rawPos * (1.0 + curveFactor);
    float voltage = minVoltage + curveY * (maxVoltage - minVoltage);
    float normalizedVoltage = (voltage - minVoltage) / (maxVoltage - minVoltage);
    int y = constrain(GRAPH_HEIGHT - (normalizedVoltage * GRAPH_HEIGHT), 0, GRAPH_HEIGHT - 1);

    display.drawPixel(x, y, WHITE);
    if (rawPos <= pressure / 50.0) {
      display.drawLine(x, GRAPH_HEIGHT - 1, x, y, WHITE);
    }
  }
}

// Função: Salva as configurações na memória flash
void saveSettings() {
  Config config = {maxVoltage, minVoltage, curveFactor};
  flashConfig.write(config);
  Serial.println("Configurações salvas na memória flash.");
}

// Função: Carrega as configurações da memória flash
void loadSettings() {
  Config config = flashConfig.read();
  maxVoltage = config.maxVoltage;
  minVoltage = config.minVoltage;
  curveFactor = config.curveFactor;

  if (maxVoltage < 2.5 || maxVoltage > 3.3) maxVoltage = 3.3;
  if (minVoltage < 1.0 || minVoltage >= maxVoltage) minVoltage = 1.47;
  if (curveFactor < -2.0 || curveFactor > 2.0) curveFactor = 0.0;

  Serial.print("Configurações carregadas: Vmax=");
  Serial.print(maxVoltage, 2);
  Serial.print("V, Vmin=");
  Serial.print(minVoltage, 2);
  Serial.print("V, Inclinação=");
  Serial.println(curveFactor, 2);
}
