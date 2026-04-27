/*
   Versão: v1.004 com Rotary Encoder para Ajustes
   Autor: Renato Brant
   Data: 2024

   Alterações nesta versão:
   1. Substituição dos potenciômetros por rotary encoder (CLK: D9, DT: D10, SW: D8).
   2. Ajuste da inclinação da curva ao girar o encoder.
   3. Modo de configuração para ajustar o nível zero (Vmax) ao pressionar o botão do encoder.
   4. Retorno automático ao modo normal após 10 segundos de inatividade no modo de configuração.
*/

#include <Wire.h>              // Biblioteca para comunicação I2C
#include <Adafruit_MCP4725.h>  // Biblioteca para o MCP4725 DAC
#include <HX711.h>             // Biblioteca para o HX711
#include <Adafruit_GFX.h>      // Biblioteca para gráficos na tela OLED
#include <Adafruit_SSD1306.h>  // Biblioteca para display OLED SSD1306
#include <Encoder.h>           // Biblioteca para o Rotary Encoder

// Definições dos pinos para o HX711, encoder e OLED
#define DT_PIN 5
#define SCK_PIN 4
#define ENCODER_CLK 9
#define ENCODER_DT 10
#define ENCODER_SW 8
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
const float MIN_VOLTAGE = 1.47;   // Tensão mínima fixa para o MCP4725 (pedal pressionado)

// Configuração do Rotary Encoder
Encoder encoder(ENCODER_CLK, ENCODER_DT);
int lastEncoderPosition = 0;
bool encoderPressed = false;

// Parâmetros ajustáveis
float maxVoltage = 3.3;           // Nível zero inicial (Vmax)
float curveFactor = 0.0;          // Inclinação da curva inicial

// Estado do sistema
enum State {
  NORMAL_MODE,
  CONFIG_VMAX
};
State currentState = NORMAL_MODE;

unsigned long lastInteraction = 0; // Para monitorar inatividade
const unsigned long INACTIVITY_TIMEOUT = 10000; // 10 segundos de inatividade

float previousPressure = 0; // Para evitar atualizações redundantes no DAC

void setup() {
  Serial.begin(9600);

  pinMode(ENCODER_SW, INPUT_PULLUP); // Configura o botão do rotary encoder

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
  // Verifica o estado atual do sistema
  if (currentState == CONFIG_VMAX) {
    configureMaxVoltage();
  } else {
    normalOperation();
  }
}

// Operação normal (ajuste da inclinação da curva)
void normalOperation() {
  // Leitura direta da pressão do pedal em kg
  float pressure = scale.get_units() / 1000.0; // Conversão para kg

  // Ajusta a inclinação da curva com o rotary encoder
  int encoderPosition = encoder.read() / 4; // Sensibilidade ajustada
  if (encoderPosition != lastEncoderPosition) {
    curveFactor += (encoderPosition - lastEncoderPosition) * 0.01;
    curveFactor = constrain(curveFactor, -1.0, 1.0); // Limita o valor entre -1 e 1
    lastEncoderPosition = encoderPosition;
    lastInteraction = millis(); // Reseta o temporizador de inatividade
  }

  // Detecta o pressionamento do botão para alternar para o modo de configuração
  if (digitalRead(ENCODER_SW) == LOW) {
    if (!encoderPressed) {
      encoderPressed = true;
      currentState = CONFIG_VMAX; // Entra no modo de configuração
      lastInteraction = millis(); // Reseta o temporizador de inatividade
      delay(300); // Previne múltiplos acionamentos
    }
  } else {
    encoderPressed = false;
  }

  // Normaliza a pressão e aplica a curva de resposta
  float normalizedPressure = pressure / 50.0; // Considera pressão máxima de 50 kg
  normalizedPressure = constrain(normalizedPressure, 0.0, 1.0); // Limita de 0 a 1

  float adjustedPressure;
  if (curveFactor > 0) {
    adjustedPressure = normalizedPressure * (1 + curveFactor);
  } else if (curveFactor < 0) {
    adjustedPressure = normalizedPressure / (1 - curveFactor);
  } else {
    adjustedPressure = normalizedPressure;
  }

  adjustedPressure = constrain(adjustedPressure, 0.0, 1.0);

  // Calcula a tensão de saída com base nos limites configurados
  float outputVoltage = MIN_VOLTAGE + (1.0 - adjustedPressure) * (maxVoltage - MIN_VOLTAGE);

  // Atualiza o DAC apenas se a pressão mudou significativamente
  if (abs(pressure - previousPressure) > 0.01) {
    uint16_t dacOutput = (uint16_t)((outputVoltage / 3.3) * 4095); // Conversão para 12 bits
    dac.setVoltage(dacOutput, false); // Envia ao DAC
    previousPressure = pressure;
  }

  // Atualiza o display OLED
  updateDisplay(pressure, outputVoltage, maxVoltage, curveFactor, normalizedPressure, "Curva:");
}

// Configuração do nível zero (Vmax)
void configureMaxVoltage() {
  int encoderPosition = encoder.read() / 4; // Sensibilidade ajustada
  if (encoderPosition != lastEncoderPosition) {
    maxVoltage += (encoderPosition - lastEncoderPosition) * 0.01; // Incrementa em 0.01V por passo
    maxVoltage = constrain(maxVoltage, 2.5, 3.3); // Limita entre 2.5V e 3.3V
    lastEncoderPosition = encoderPosition;
    lastInteraction = millis(); // Reseta o temporizador de inatividade
  }

  // Detecta o pressionamento do botão para voltar ao modo normal
  if (digitalRead(ENCODER_SW) == LOW) {
    if (!encoderPressed) {
      encoderPressed = true;
      currentState = NORMAL_MODE; // Retorna ao modo normal
      delay(300); // Previne múltiplos acionamentos
    }
  } else {
    encoderPressed = false;
  }

  // Retorna ao modo normal após 10 segundos de inatividade
  if (millis() - lastInteraction > INACTIVITY_TIMEOUT) {
    currentState = NORMAL_MODE;
  }

  // Atualiza o display OLED
  updateDisplay(0, 0, maxVoltage, 0, 0, "Ajustando Vmax:");
}

// Atualiza o display OLED
void updateDisplay(float pressure, float voltage, float maxVoltage, float curveFactor, float normalizedPressure, const char *status) {
  display.clearDisplay();
  display.setCursor(0, 0);

  display.println(status);
  display.print("Pressao: ");
  display.print(pressure, 1);
  display.println(" kg");

  display.print("Saida: ");
  display.print(voltage, 2);
  display.println(" V");

  display.print("Vmax: ");
  display.print(maxVoltage, 2);
  display.println(" V");

  display.print("Curva: ");
  display.print(curveFactor, 2);

  display.display();
}
