/*
  Programa: Simulador de Freio com Saída Analógica
  Autor: Renato Brant
  Data: 01/12/2024
  Versão: v2.006

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
  float maxVoltage;
  float minVoltage;
  float curveFactor;
};
FlashStorage(flashConfig, Config); // Aloca espaço na flash

// Configurações iniciais
float maxVoltage = 3.3;           // Tensão máxima inicial
float minVoltage = 1.47;          // Tensão mínima inicial
float curveFactor = 0.0;          // Inclinação inicial
int lastEncoderPosition = 0;
bool buttonPressed = false;
int configMode = 0; // 0 = Vmax, 1 = Vmin, 2 = Inclinação

// Configuração do HX711
const float CALIBRATION_FACTOR = -44.765419; // Fator de calibração
const long OFFSET = 1877599;                 // Offset para zero
float pressure = 0;                          // Pressão medida em kg

// Dados do gráfico
const int GRAPH_WIDTH = 122;      // Largura do gráfico
const int GRAPH_HEIGHT = 64;      // Altura do gráfico

float previousVoltage = 0;       // Última tensão registrada para sincronização

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

  // Carrega configurações da flash
  loadSettings();

  Serial.println("Sistema inicializado com sucesso!");
}

void loop() {
  // Processa entrada do botão do encoder
  handleButtonPress();

  // Processa movimentação do encoder rotativo
  handleEncoderRotation();

  // Leitura e atualização
  if (scale.is_ready()) {
    pressure = readPressure();
    updateDAC();
    updateDisplay();
  }
}

void handleButtonPress() {
  bool currentButtonState = digitalRead(ROTARY_SW_PIN) == LOW;
  if (currentButtonState && !buttonPressed) {
    configMode = (configMode + 1) % 3; // Alterna entre Vmax, Vmin e Inclinação
    saveSettings();
  }
  buttonPressed = currentButtonState;
}

void handleEncoderRotation() {
  int newEncoderPosition = rotaryEncoder.read() / 2; // Divide por 2 para maior controle
  if (newEncoderPosition != lastEncoderPosition) {
    int delta = newEncoderPosition - lastEncoderPosition;

    if (configMode == 0) {
      maxVoltage += delta * 0.05; // Ajuste de Vmax
      maxVoltage = constrain(maxVoltage, 2.5, 3.3);
    } else if (configMode == 1) {
      minVoltage += delta * 0.05; // Ajuste de Vmin
      minVoltage = constrain(minVoltage, 1.0, maxVoltage - 0.1);
    } else if (configMode == 2) {
      curveFactor += delta * 0.01; // Ajuste de inclinação
      curveFactor = constrain(curveFactor, -1.0, 1.0);
    }

    lastEncoderPosition = newEncoderPosition;
  }
}

float readPressure() {
  float measuredPressure = scale.get_units() / 1000.0; // Converte para kg
  return constrain(measuredPressure, 0.0, 50.0); // Limita a pressão entre 0 e 50 kg
}

void updateDAC() {
  float normalizedPressure = pressure / 50.0; // Pressão máxima de 50 kg
  normalizedPressure = constrain(normalizedPressure, 0.0, 1.0);

  float adjustedPressure = normalizedPressure * (1.0 + curveFactor);
  adjustedPressure = constrain(adjustedPressure, 0.0, 1.0);

  float outputVoltage = minVoltage + (1.0 - adjustedPressure) * (maxVoltage - minVoltage);
  uint16_t dacOutput = (uint16_t)((outputVoltage / 3.3) * 4095);

  dac.setVoltage(dacOutput, false);
  previousVoltage = outputVoltage; // Salva o valor atual da tensão
}

void updateDisplay() {
  display.clearDisplay();
  drawGraph();

  // Exibe Vmax no canto inferior esquerdo
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(maxVoltage, 2);
  display.print("V");

  // Destaca Vmax se estiver sendo configurado
  if (configMode == 0) {
    display.drawRect(0, 53, 30, 10, WHITE);
  }

  // Exibe Vmin no canto superior direito
  display.setCursor(100, 0);
  display.print(minVoltage, 2);
  display.print("V");

  // Destaca Vmin se estiver sendo configurado
  if (configMode == 1) {
    display.drawRect(99, -1, 30, 10, WHITE);
  }

  // Exibe a inclinação no canto superior esquerdo
  display.setCursor(0, 20);
  display.print("I:");
  display.print(curveFactor, 2);

  // Destaca inclinação se estiver sendo configurada
  if (configMode == 2) {
    display.drawRect(0, 19, 50, 10, WHITE);
  }

  // Exibe fundo preto e pressão no canto inferior direito (movido um caractere para a direita)
  display.fillRect(66, 32, 58, 16, BLACK);  // Movido levemente para a direita
  display.setTextColor(WHITE, BLACK);
  display.setCursor(66, 32);                // Posicionado mais à direita
  display.setTextSize(2);                   // Mantido tamanho 2
  display.print(String(pressure, 0) + "kg");

  // Exibe fundo preto e tensão de saída logo abaixo da pressão
  display.fillRect(60, 48, 58, 16, BLACK);  // Mantido alinhamento original
  display.setCursor(60, 48);                // Posicionado corretamente
  display.setTextSize(2);                   // Mantido tamanho 2
  display.print(String(previousVoltage, 2) + "V");

  display.display();
}





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

void saveSettings() {
  Config config = {maxVoltage, minVoltage, curveFactor};
  flashConfig.write(config);
  Serial.println("Configurações salvas na memória flash.");
}

void loadSettings() {
  Config config = flashConfig.read();
  maxVoltage = config.maxVoltage;
  minVoltage = config.minVoltage;
  curveFactor = config.curveFactor;

  if (maxVoltage < 2.5 || maxVoltage > 3.3) {
    maxVoltage = 3.3;
  }
  if (minVoltage < 1.0 || minVoltage >= maxVoltage) {
    minVoltage = 1.47;
  }
  if (curveFactor < -1.0 || curveFactor > 1.0) {
    curveFactor = 0.0;
  }

  Serial.print("Configurações carregadas: Vmax=");
  Serial.print(maxVoltage, 2);
  Serial.print("V, Vmin=");
  Serial.print(minVoltage, 2);
  Serial.print("V, Inclinação=");
  Serial.println(curveFactor, 2);
}
