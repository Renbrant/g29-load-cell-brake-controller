#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <HX711.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <FlashStorage.h> // Substituição da EEPROM

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
  float curveFactor;
};
FlashStorage(flashConfig, Config); // Aloca espaço na flash

// Configuração inicial
float maxVoltage = 3.3;           // Tensão máxima inicial
float curveFactor = 0.0;          // Fator de curva inicial
int lastEncoderPosition = 0;
bool buttonPressed = false;
bool isAdjustingVmax = true;      // Alterna entre ajustar `Vmax` e `Curva`

// Configuração do HX711
const float CALIBRATION_FACTOR = -44.765419; // Fator de calibração
const long OFFSET = 1877599;                 // Offset para zero
float pressure = 0;                          // Pressão medida em kg

// Dados do gráfico
const int GRAPH_WIDTH = 122;      // Largura do gráfico
const int GRAPH_HEIGHT = 64;      // Altura do gráfico
const float MIN_VOLTAGE = 1.47;   // Tensão mínima fixa para o MCP4725

float previousPressure = 0;       // Última pressão registrada para comparação

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
  static unsigned long lastDisplayUpdate = 0;

  // Leitura do estado do botão do encoder
  bool currentButtonPressed = digitalRead(ROTARY_SW_PIN) == LOW;
  if (currentButtonPressed && !buttonPressed) {
    isAdjustingVmax = !isAdjustingVmax; // Alterna entre ajustar Vmax e Curva
    Serial.println(isAdjustingVmax ? "Ajustando Vmax" : "Ajustando Curva");
    saveSettings(); // Salva as configurações na flash
  }
  buttonPressed = currentButtonPressed;

  // Leitura do encoder rotativo
  int newEncoderPosition = rotaryEncoder.read() / 2; // Divide por 2 para maior controle
  if (newEncoderPosition != lastEncoderPosition) {
    int delta = newEncoderPosition - lastEncoderPosition;

    if (isAdjustingVmax) {
      // Ajusta o Vmax
      maxVoltage += delta * 0.05; // Incrementa ou decrementa em passos de 0.05
      maxVoltage = constrain(maxVoltage, 2.5, 3.3);
      Serial.print("Vmax ajustado para: ");
      Serial.println(maxVoltage, 2);
    } else {
      // Ajusta o fator de curva
      curveFactor += delta * 0.01; // Incrementa ou decrementa em passos de 0.01
      curveFactor = constrain(curveFactor, -1.0, 1.0);
      Serial.print("Curva ajustada para: ");
      Serial.println(curveFactor, 2);
    }

    lastEncoderPosition = newEncoderPosition;
  }

  // Leitura da pressão do pedal
  if (scale.is_ready()) {
    pressure = scale.get_units() / 1000.0; // Converte para kg
    pressure = constrain(pressure, 0.0, 50.0); // Limita a pressão entre 0 e 50 kg
  }

  // Atualiza a saída do DAC
  updateDAC();

  // Atualiza o display OLED a cada 200 ms
  if (millis() - lastDisplayUpdate > 200) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

void updateDAC() {
  // Normaliza a pressão e aplica a curva de resposta
  float normalizedPressure = pressure / 50.0; // Pressão máxima de 50 kg
  normalizedPressure = constrain(normalizedPressure, 0.0, 1.0);

  float adjustedPressure;
  if (curveFactor > 0) {
    adjustedPressure = normalizedPressure * (1 + curveFactor);
  } else if (curveFactor < 0) {
    adjustedPressure = normalizedPressure / (1 - curveFactor);
  } else {
    adjustedPressure = normalizedPressure;
  }
  adjustedPressure = constrain(adjustedPressure, 0.0, 1.0);

  // Ajusta Vout com os novos limites
  float outputVoltage = MIN_VOLTAGE + (1.0 - adjustedPressure) * (maxVoltage - MIN_VOLTAGE);

  // Atualiza o DAC apenas se a pressão mudou significativamente
  if (abs(pressure - previousPressure) > 0.01) {
    uint16_t dacOutput = (uint16_t)((outputVoltage / 3.3) * 4095); // Conversão para 12 bits
    dac.setVoltage(dacOutput, false);
    previousPressure = pressure;
  }
}

void updateDisplay() {
  display.clearDisplay();

  // Desenha o gráfico primeiro (em todo o espaço da tela)
  drawGraph();

  // Exibe o valor de pressão no canto superior esquerdo
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(WHITE, BLACK); // Texto branco com fundo preto para sobrepor o gráfico
  display.print(pressure, 0);
   display.setTextSize(1);
  display.print("kg");

  // Exibe o valor atual de configuração logo abaixo
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print(isAdjustingVmax ? "Vmax: " : "Curva: ");
  display.print(isAdjustingVmax ? maxVoltage : curveFactor, 2);

  // Atualiza o display
  display.display();
}

void drawGraph() {
  // O gráfico ocupa toda a área da tela
  display.fillRect(1, 0, GRAPH_WIDTH - 1, GRAPH_HEIGHT, BLACK);

  // Calcula a pressão normalizada (0 a 1)
  float normalizedPressure = pressure / 50.0;

  for (int x = 0; x < GRAPH_WIDTH; x++) {
    float rawPos = (float)x / GRAPH_WIDTH;
    float curveY;

    // Calcula o gráfico com base na curva ajustada
    if (curveFactor > 0) {
      curveY = rawPos * (1 + curveFactor);
    } else if (curveFactor < 0) {
      curveY = rawPos / (1 - curveFactor);
    } else {
      curveY = rawPos;
    }

    curveY = constrain(curveY, 0.0, 1.0);
    int y = constrain(GRAPH_HEIGHT - (curveY * GRAPH_HEIGHT), 0, GRAPH_HEIGHT - 1);

    // Desenha o gráfico
    display.drawPixel(x, y, WHITE);

    // Preenche até o nível de pressão atual
    if (rawPos <= normalizedPressure) {
      display.drawLine(x, GRAPH_HEIGHT - 1, x, y, WHITE);
    }
  }
}



void saveSettings() {
  Config config = {maxVoltage, curveFactor};
  flashConfig.write(config);
  Serial.println("Configurações salvas na memória flash.");
}

void loadSettings() {
  Config config = flashConfig.read();
  maxVoltage = config.maxVoltage;
  curveFactor = config.curveFactor;

  // Validação simples
  if (maxVoltage < 2.5 || maxVoltage > 3.3) {
    maxVoltage = 3.3;
  }
  if (curveFactor < -1.0 || curveFactor > 1.0) {
    curveFactor = 0.0;
  }

  Serial.print("Configurações carregadas: Vmax=");
  Serial.print(maxVoltage, 2);
  Serial.print("V, Curva=");
  Serial.println(curveFactor, 2);
}
