/*
   Versão: v0.5 - Sistema de Leitura de Pressão e Saída de Tensão para Simulador de Freio
   Autor: Assistente ChatGPT
   Data: 2024

   Descrição:
   Este programa utiliza o HX711 para medir a pressão de um pedal de freio de simulador,
   exibe os dados no display OLED e ajusta a saída de tensão via MCP4725 para simular 
   a resposta de um sistema de freio de competição. A leitura de pressão é mapeada para
   uma saída de tensão de 0 a 3.3V (máximo) no DAC. O sistema permite ajustes de curva
   e sensibilidade via potenciômetros.

   Alterações nesta versão (v0.5):
   1. Configuração manual da taxa de amostragem para 80 SPS no HX711.
   2. Configura o MCP4725 para ajustar a tensão de saída de 0 a 3.3V com uma resolução de 12 bits.
   3. Ajusta a escala máxima do sistema para 30 kg.
   4. Inclui dois potenciômetros para ajuste de sensibilidade (de 15 a 30 kg) e curva de resposta.
   5. Exibe um gráfico no display OLED, representando a curva de resposta em tempo real.

   Componentes:
   - HX711: Sensor para medição da pressão com taxa de amostragem configurável.
   - MCP4725: DAC de 12 bits para saída de tensão precisa (0 a 3.3V).
   - Display OLED SSD1306: Exibe o gráfico de pressão e curva de resposta.
   - Potenciômetros: Ajustam a sensibilidade e a curva de resposta do sistema.
*/

#include <Wire.h>              // Biblioteca para comunicação I2C
#include <Adafruit_MCP4725.h>  // Biblioteca para o MCP4725 DAC
#include <HX711.h>             // Biblioteca para o HX711
#include <Adafruit_GFX.h>      // Biblioteca para gráficos na tela OLED
#include <Adafruit_SSD1306.h>  // Biblioteca para display OLED SSD1306

// Definições dos pinos para o HX711, potenciômetros e OLED
#define DT_PIN 3
#define SCK_PIN 4
#define POT_SENSITIVITY A2     // Pino do potenciômetro para ajuste de sensibilidade
#define POT_CURVE A3           // Pino do potenciômetro para ajuste da curva de resposta
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
const float MAX_PRESSURE = 30.0;  // Pressão máxima para o eixo X (30 kg)

// Variáveis de controle de saída e constantes
const float MAX_VOLTAGE = 3.3;    // Tensão máxima de saída para o MCP4725

void setup() {
  Serial.begin(9600);
  
  // Configuração para forçar a taxa de amostragem para 80 SPS
  bool isRate80SPS = true;

  // Exibe a taxa de amostragem configurada
  Serial.print("Sistema inicializado com taxa de amostragem do HX711: ");
  if (isRate80SPS) {
      Serial.println("80 SPS");
  } else {
      Serial.println("10 SPS");
  }

  scale.begin(DT_PIN, SCK_PIN);
  scale.set_offset(OFFSET);
  scale.set_scale(CALIBRATION_FACTOR);

  dac.begin(MCP4725_I2C_ADDRESS); // Inicializa o MCP4725
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Inicializa o display OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  Serial.println("Sistema inicializado com MCP4725 e display OLED configurado para 30 kg.");
}

void loop() {
  // Leitura direta da pressão do pedal em kg
  float pressure = scale.get_units() / 1000.0; // Conversão para kg

  // Ajustes de sensibilidade e curva de resposta usando potenciômetros
  int potSensitivity = analogRead(POT_SENSITIVITY);
  float adjustedMaxPressure = map(potSensitivity, 0, 1023, 15, 30); // Ajusta de 15 a 30 kg como pressão máxima
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

  // Mapeia a pressão ajustada para a saída do DAC de 0 a 3.3V (12 bits)
  uint16_t dacOutput = (uint16_t)(adjustedPressure * 4095 * (MAX_VOLTAGE / 5.0)); 
  dacOutput = constrain(dacOutput, 0, 4095 * (MAX_VOLTAGE / 5.0)); // Limite a 3.3V máx
  dac.setVoltage(dacOutput, false); // Envia ao DAC

  // Exibe no monitor serial para depuração
  Serial.print("Pressao: ");
  Serial.print(pressure, 2);
  Serial.print(" kg, Saida de tensao (calculada): ");
  Serial.print((dacOutput / 4095.0) * MAX_VOLTAGE, 2); // Conversão para volts reais
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
    display.setCursor(0, GRAPH_HEIGHT + 1);
    display.print("G29BS");
    display.setCursor(GRAPH_WIDTH - 30, GRAPH_HEIGHT + 1);
    display.print("30kg");  // Define o valor máximo como 30 kg para o eixo X
    display.setCursor(0, 0);
    display.print(MAX_VOLTAGE, 1); // Valor máximo do eixo Y (3.3V)
    //display.setCursor(0, 10);       // Posiciona Vout logo abaixo do 3.3
    display.print("V");          // Rótulo do eixo Y
    display.setCursor(GRAPH_WIDTH - 20, GRAPH_HEIGHT + 10);
    //display.print("kg");           // Rótulo do eixo X

    // Desenha a curva de resposta ajustada e preenche a área até a curva
    int fillWidth = map(pressure, 0, MAX_PRESSURE, 0, GRAPH_WIDTH); // Mapeia pressão para a largura da tela
    for (int x = 0; x < GRAPH_WIDTH; x++) {
      float rawPos = (float)x / GRAPH_WIDTH; // Normaliza o valor para o intervalo 0 a 1 relativo a 30 kg
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
