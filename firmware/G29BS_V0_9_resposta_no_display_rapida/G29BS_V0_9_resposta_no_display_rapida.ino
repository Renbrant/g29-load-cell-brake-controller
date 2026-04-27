/*
   Versão: v1.1 com Feedback Visual para Potenciômetros
   Autor: Assistente ChatGPT
   Data: 2024

   Alterações nesta versão:
   1. Adicionado feedback visual com barras de progresso para os potenciômetros.
   2. Ajustado o gráfico para acomodar as barras laterais sem impactar o desempenho.
   3. Verificação e validação de limites para evitar artefatos no display.
*/

#include <Wire.h>              // Biblioteca para comunicação I2C
#include <Adafruit_MCP4725.h>  // Biblioteca para o MCP4725 DAC
#include <HX711.h>             // Biblioteca para o HX711
#include <Adafruit_GFX.h>      // Biblioteca para gráficos na tela OLED
#include <Adafruit_SSD1306.h>  // Biblioteca para display OLED SSD1306

// Definições dos pinos para o HX711, potenciômetros e OLED
#define DT_PIN 5
#define SCK_PIN 4
#define POT_SENSITIVITY A0     // Pino do potenciômetro para ajuste de sensibilidade
#define POT_CURVE A1           // Pino do potenciômetro para ajuste da curva de resposta
#define OLED_RESET -1          // Reset do display OLED, não necessário no Nano

// Configuração do HX711
HX711 scale;
const float CALIBRATION_FACTOR = -44.765419; // Fator de calibração da célula de carga
const long OFFSET = 1877599;                 // Offset inicial para zero

// Configuração do MCP4725
Adafruit_MCP4725 dac;
const int MCP4725_I2C_ADDRESS = 0x60; // Endereço padrão I2C do MCP4725

// Configuração do display OLED
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
const int GRAPH_WIDTH = 122;      // Largura do gráfico ajustada (em pixels)
const int GRAPH_HEIGHT = 64;      // Altura do gráfico (em pixels)
const float MAX_PRESSURE = 50.0;  // Pressão máxima para o eixo X (50 kg)

// Variáveis de controle de saída e constantes
const float MAX_VOLTAGE = 2.89;    // Tensão máxima de saída para o MCP4725 com pedal solto
const float MIN_VOLTAGE = 1.47;    // Tensão mínima de saída para o MCP4725 com pedal pressionado

void setup() {
  Serial.begin(9600);

  scale.begin(DT_PIN, SCK_PIN);
  scale.set_offset(OFFSET);
  scale.set_scale(CALIBRATION_FACTOR);

  dac.begin(MCP4725_I2C_ADDRESS); // Inicializa o MCP4725
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Inicializa o display OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  Serial.println("Sistema inicializado com MCP4725 e display OLED configurado.");
}

void loop() {
  // Leitura direta da pressão do pedal em kg
  float pressure = scale.get_units() / 1000.0; // Conversão para kg

  // Ajustes de sensibilidade e curva de resposta usando potenciômetros
  int potSensitivity = analogRead(POT_SENSITIVITY);
  float adjustedMaxPressure = map(potSensitivity, 0, 1023, 5, 50); // Ajusta de 5 a 50 kg como pressão máxima
  int potCurve = analogRead(POT_CURVE);
  float curveFactor = (float)map(potCurve, 0, 1023, -100, 100) / 100.0;

  // Normaliza a pressão e aplica a curva de resposta
  float normalizedPressure = pressure / adjustedMaxPressure;
  normalizedPressure = constrain(normalizedPressure, 0.0, 1.0); // Limite de 0 a 1
  float adjustedPressure;

  // Aplica curva com base no fator
  if (curveFactor > 0) {
    adjustedPressure = pow(normalizedPressure, 1.0 + curveFactor);
  } else if (curveFactor < 0) {
    adjustedPressure = 1 - pow(1 - normalizedPressure, 1.0 - curveFactor);
  } else {
    adjustedPressure = normalizedPressure;
  }

  // Inversão e ajuste para o range 2.89V (solto) e 1.47V (pressionado)
  float outputVoltage = MIN_VOLTAGE + (1.0 - adjustedPressure) * (MAX_VOLTAGE - MIN_VOLTAGE);
  uint16_t dacOutput = (uint16_t)((outputVoltage / 3.3) * 4095); // Conversão para 12 bits
  dac.setVoltage(dacOutput, false); // Envia ao DAC

  // Atualiza o display OLED a cada 100 ms
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 100) {
    lastUpdate = millis();
    updateDisplay(pressure, outputVoltage, adjustedMaxPressure, curveFactor, normalizedPressure);
  }
}

void updateDisplay(float pressure, float voltage, float maxPressure, float curveFactor, float normalizedPressure) {
  // Limpa apenas a área gráfica
  display.fillRect(1, 0, GRAPH_WIDTH - 1, GRAPH_HEIGHT, BLACK);

  // Exibe valores no canto superior esquerdo
  display.setCursor(3, 1);
  display.print(pressure, 0);
  display.print("kg ");
  display.setTextSize(1);
  display.setCursor(3, 18);
  display.print(voltage, 2);
  display.print("V");
  display.setTextSize(2);

  // Feedback Visual para Potenciômetros
  // Barra para Sensibilidade (POT_SENSITIVITY)
  int sensitivityHeight = map(analogRead(POT_SENSITIVITY), 0, 1023, 0, GRAPH_HEIGHT / 2);
  display.fillRect(GRAPH_WIDTH + 1, GRAPH_HEIGHT / 2 - sensitivityHeight, 3, sensitivityHeight, WHITE);

  // Barra para Curva (POT_CURVE)
  int curveHeight = map(analogRead(POT_CURVE), 0, 1023, 0, GRAPH_HEIGHT / 2);
  display.fillRect(GRAPH_WIDTH + 1, GRAPH_HEIGHT - curveHeight, 3, curveHeight, WHITE);

  // Desenha o gráfico da curva com preenchimento inferior
  for (int x = 0; x < GRAPH_WIDTH; x += 2) {
    float rawPos = (float)x / GRAPH_WIDTH;
    float curveY;
    if (curveFactor > 0) {
      curveY = pow(rawPos, 1.0 + curveFactor);
    } else if (curveFactor < 0) {
      curveY = 1 - pow(1 - rawPos, 1.0 - curveFactor);
    } else {
      curveY = rawPos;
    }

    int y = constrain(GRAPH_HEIGHT - (curveY * GRAPH_HEIGHT), 0, GRAPH_HEIGHT - 1);

    // Desenho seguro de pixels
    if (x >= 0 && x < GRAPH_WIDTH && y >= 0 && y < GRAPH_HEIGHT) {
      display.drawPixel(x, y, WHITE);
    }

    // Preenche até o nível de pressão atual
    if (rawPos <= normalizedPressure) {
      display.drawLine(x, GRAPH_HEIGHT - 1, x, y, WHITE);
    }
  }

  display.display();
}
