#include <HX711.h>            // Biblioteca para o amplificador HX711
#include <Wire.h>             // Biblioteca para comunicação I2C
#include <Adafruit_GFX.h>     // Biblioteca para gráficos na tela OLED
#include <Adafruit_SSD1306.h> // Biblioteca para display OLED SSD1306

// Definições dos pinos
#define DT_PIN 3              // Pino DT do HX711 no pino digital 3 do Arduino Nano
#define SCK_PIN 4             // Pino SCK do HX711 no pino digital 4 do Arduino Nano
#define OLED_RESET -1         // Reset do display OLED, não necessário no Nano
#define ANALOG_OUT A1         // Saída analógica para a porta A1
#define POT_SENSITIVITY A2    // Pino do potenciômetro para ajuste de sensibilidade
#define POT_CURVE A3          // Pino do potenciômetro para ajuste da curva de resposta

// Configurações para o display OLED
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

// Configuração do HX711
HX711 scale;

// Constantes para o mapeamento
const float MAX_VOLTAGE = 3.3;   // Tensão máxima desejada
const int BASE_RAW_MAX = 2100;   // Valor máximo de referência para RAW sem ajuste
const int GRAPH_WIDTH = 128;     // Largura do gráfico (em pixels)
const int GRAPH_HEIGHT = 48;     // Altura do gráfico (em pixels)

// Variáveis de calibração
long rawMin = 0;
long rawMax = 0;

void setup() {
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Endereço I2C padrão do display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  scale.begin(DT_PIN, SCK_PIN);
  Serial.println("Sistema inicializado. Digite 'C' e pressione Enter para iniciar a calibração.");
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'c' || command == 'C') {
      calibrate();
    }
  }

  // Lê o valor bruto (RAW) do HX711
  long rawValue = scale.read();

  // Ajusta o valor de RAW para o intervalo entre rawMin e rawMax
  long adjustedRawMax = rawMax > rawMin ? rawMax : BASE_RAW_MAX;
  float normalizedRaw = (float)(rawValue - rawMin) / (adjustedRawMax - rawMin);

  // Aplica a curva de resposta
  int potCurve = analogRead(POT_CURVE);
  float curveFactor = (float)map(potCurve, 0, 1023, -100, 100) / 100.0;
  float adjustedValue = applyCurve(normalizedRaw, curveFactor);

  // Mapeia o valor ajustado para PWM
  int pwmValue = map(adjustedValue * adjustedRawMax, 0, adjustedRawMax, 0, (int)(255 * MAX_VOLTAGE / 5.0));
  pwmValue = constrain(pwmValue, 0, (int)(255 * MAX_VOLTAGE / 5.0)); // Limita PWM para 3.3V máx

  analogWrite(ANALOG_OUT, pwmValue);

  // Exibe informações de depuração na Serial
  Serial.print("RAW: ");
  Serial.print(rawValue);
  Serial.print(", SensMax: ");
  Serial.print(adjustedRawMax);
  Serial.print(", CFactor: ");
  Serial.print(curveFactor, 2);
  Serial.print(", AdjValue: ");
  Serial.print(adjustedValue, 4);
  Serial.print(", PWM: ");
  Serial.print(pwmValue);
  Serial.println();

  delay(100); // Controle de atualização
}

void calibr
