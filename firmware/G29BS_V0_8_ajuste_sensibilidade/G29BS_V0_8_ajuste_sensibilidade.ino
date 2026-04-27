/*
   Versão: v0.8 Modificada - Sistema de Leitura de Pressão e Saída de Tensão para Simulador de Freio
   Autor: Assistente ChatGPT
   Data: 2024

   Descrição:
   Este programa utiliza o HX711 para medir a pressão de um pedal de freio de simulador,
   exibe os dados no display OLED e ajusta a saída de tensão via MCP4725 para simular 
   a resposta de um sistema de freio de competição. A leitura de pressão é mapeada para
   uma saída de tensão de 2.89V (mínimo de pressão) a 1.47V (máximo de pressão) no DAC.

   Alterações nesta versão:
   1. Range de saída do MCP4725 ajustado para 2.89V a 1.47V.
   2. Pressão máxima ajustada para 50 kg.
   3. Ajuste dos potenciômetros para uma pressão máxima ajustável entre 5 e 50 kg.
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
const int GRAPH_WIDTH = 128;      // Largura do gráfico (em pixels)
const int GRAPH_HEIGHT = 48;      // Altura do gráfico (em pixels)
const float MAX_PRESSURE = 50.0;  // Pressão máxima para o eixo X (50 kg)

// Variáveis de controle de saída e constantes
const float MAX_VOLTAGE = 2.89;    // Tensão máxima de saída para o MCP4725 com pedal solto
const float MIN_VOLTAGE = 1.47;    // Tensão mínima de saída para o MCP4725 com pedal pressionado

void setup() {
  Serial.begin(9600);

  // Configuração para forçar a taxa de amostragem para 80 SPS
  bool isRate80SPS = true;

  // Exibe a taxa de amostragem configurada
  Serial.print("Sistema inicializado com taxa de amostragem do HX711: ");
  Serial.println(isRate80SPS ? "80 SPS" : "10 SPS");

  scale.begin(DT_PIN, SCK_PIN);
  scale.set_offset(OFFSET);
  scale.set_scale(CALIBRATION_FACTOR);

  dac.begin(MCP4725_I2C_ADDRESS); // Inicializa o MCP4725
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Inicializa o display OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  Serial.println("Sistema inicializado com MCP4725 e display OLED configurado para 50 kg.");
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

  // Exibe no monitor serial para depuração
  Serial.print("Pressao: ");
  Serial.print(pressure, 2);
  Serial.print(" kg, Saida de tensao (calculada): ");
  Serial.print(outputVoltage, 2); // Conversão para volts reais
  Serial.print(" V, Saida DAC: ");
  Serial.println(dacOutput);

  // Atualiza o display OLED a cada 500 ms
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();
    display.clearDisplay();
  
    // Eixos do gráfico
    display.drawLine(0, GRAPH_HEIGHT, GRAPH_WIDTH, GRAPH_HEIGHT, WHITE); // Eixo X
    display.drawLine(0, 0, 0, GRAPH_HEIGHT, WHITE);                     // Eixo Y

    // Rótulos dos eixos e valores máximos
    display.setCursor(1, GRAPH_HEIGHT + 1);
    display.print("G29BS");
    display.setCursor(GRAPH_WIDTH - 30, GRAPH_HEIGHT + 1);
    display.print("50kg");  // Define o valor máximo como 50 kg para o eixo X
    display.setCursor(1, 1);
    display.print(MAX_VOLTAGE, 1); // Valor máximo do eixo Y (2.89V)
    display.print("V");          // Rótulo do eixo Y

    // Desenha a curva de resposta ajustada e preenche a área até a curva
    int fillWidth = map(pressure, 0, MAX_PRESSURE, 0, GRAPH_WIDTH); // Mapeia pressão para a largura da tela
    for (int x = 0; x < GRAPH_WIDTH; x++) {
      float rawPos = (float)x / GRAPH_WIDTH; // Normaliza o valor para o intervalo 0 a 1 relativo a 50 kg
      float curveY;

      // Ajuste de curva com base em adjustedMaxPressure
      if (rawPos <= (float)adjustedMaxPressure / MAX_PRESSURE) {
        float adjustedRawPos = rawPos / ((float)adjustedMaxPressure / MAX_PRESSURE);

        // Calcula a curva ajustada
        if (curveFactor > 0) {
          curveY = pow(adjustedRawPos, 1.0 + curveFactor);
        } else if (curveFactor < 0) {
          curveY = 1 - pow(1 - adjustedRawPos, 1.0 - curveFactor);
        } else {
          curveY = adjustedRawPos;
        }
      } else {
        curveY = 1.0;  // Curva horizontal após adjustedMaxPressure
      }

      int y = GRAPH_HEIGHT - (curveY * GRAPH_HEIGHT);
      display.drawPixel(x, y, WHITE);

      // Preenche a área inferior até a curva, somente até o valor atual de pressão
      if (x <= fillWidth) {
        display.drawLine(x, GRAPH_HEIGHT, x, y, WHITE);
      }
    }

    display.display();
  }
}
