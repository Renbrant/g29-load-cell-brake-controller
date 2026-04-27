#include <HX711.h>            // Biblioteca para o amplificador HX711
#include <Wire.h>             // Biblioteca para comunicação I2C
#include <Adafruit_GFX.h>     // Biblioteca para gráficos na tela OLED
#include <Adafruit_SSD1306.h> // Biblioteca para display OLED SSD1306

// Definições dos pinos
#define DT_PIN 3       // Pino DT do HX711 no pino digital 3 do Arduino Nano
#define SCK_PIN 4      // Pino SCK do HX711 no pino digital 4 do Arduino Nano
#define OLED_RESET -1  // Reset do display OLED, não necessário no Nano
#define ANALOG_OUT A1  // Saída analógica para a porta A1
#define POT_SENSITIVITY A2 // Potenciômetro para ajuste de sensibilidade
#define POT_CURVE A3       // Potenciômetro para ajuste da curva de resposta

// Configuração do display OLED
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
HX711 scale;

// Configurações iniciais para o HX711
const float MAX_VOLTAGE = 3.3;      // Limite de tensão para o pedal
const float CALIBRATION_FACTOR = -44.765419; // Fator de calibração da célula de carga
const long OFFSET = 1877599;        // Offset inicial para zero
const int GRAPH_WIDTH = 128;        // Largura do gráfico (em pixels)
const int GRAPH_HEIGHT = 48;        // Altura do gráfico (em pixels)

float maxWeightDetected = 0.0; // Variável para armazenar o valor máximo de peso detectado

void setup() {
  Serial.begin(9600);
  scale.begin(DT_PIN, SCK_PIN);
  scale.set_offset(OFFSET);
  scale.set_scale(CALIBRATION_FACTOR);
  
  // Realiza a tara para zerar a leitura
  scale.tare();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  Serial.println("Sistema inicializado. Pedal de freio em operação...");
}

void loop() {
  // Leitura do peso em kg
  float weight = scale.get_units(10) / 1000.0; // Divide por 1000 para converter gramas para kg

  // Atualiza o valor máximo detectado
  if (weight > maxWeightDetected) {
    maxWeightDetected = weight;
  }
  
  // Ajuste de sensibilidade com potenciômetro
  int potValue = analogRead(POT_SENSITIVITY);
  float sensitivityFactor = map(potValue, 0, 1023, 0.5, 2.0); // Ajusta a sensibilidade
  
  // Ajuste de curva com potenciômetro
  int potCurve = analogRead(POT_CURVE);
  float curveFactor = (float)map(potCurve, 0, 1023, -100, 100) / 100.0; // Valor entre -1 e +1 para controle da curva

  // Aplica a curva de resposta ao peso normalizado
  float normalizedWeight = weight / maxWeightDetected; // Normaliza o peso entre 0 e 1
  float adjustedWeight;
  if (curveFactor > 0) {
    // Curva exponencial
    adjustedWeight = pow(normalizedWeight, 1.0 + curveFactor);
  } else if (curveFactor < 0) {
    // Curva logarítmica invertida
    adjustedWeight = 1 - pow(1 - normalizedWeight, 1.0 - curveFactor);
  } else {
    // Curva linear
    adjustedWeight = normalizedWeight;
  }
  
  // Ajusta o peso final com o fator de sensibilidade
  adjustedWeight *= sensitivityFactor * maxWeightDetected;

  // Calcula o valor de saída PWM com base no peso ajustado
  int pwmOutput = map(adjustedWeight, 0, maxWeightDetected, 0, 255);
  pwmOutput = constrain(pwmOutput, 0, 255); // Limita a saída para 3.3V (255 de PWM)

  analogWrite(ANALOG_OUT, pwmOutput);

  // Exibe no monitor serial
  Serial.print("Peso: ");
  Serial.print(weight, 2);
  Serial.print(" kg, Saída: ");
  Serial.print((float)pwmOutput * (MAX_VOLTAGE / 255), 2); // Conversão para volts
  Serial.println(" V");

  // Atualiza o display OLED
  display.clearDisplay();
  
  // Exibe o valor do peso e a saída de tensão no OLED
  display.setCursor(0, 0);
  display.print("Peso:");
  display.setCursor(0, 16);
  display.setTextSize(2);
  display.print(weight, 2);
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("Saida em V:");
  display.setCursor(0, 50);
  display.print((float)pwmOutput * (MAX_VOLTAGE / 255), 2);

  // Desenha o gráfico de resposta no OLED
  display.drawLine(0, GRAPH_HEIGHT, GRAPH_WIDTH, GRAPH_HEIGHT, WHITE); // Eixo X
  display.drawLine(0, 0, 0, GRAPH_HEIGHT, WHITE);                      // Eixo Y

  // Rótulos dos eixos e valores máximos
  display.setCursor(0, GRAPH_HEIGHT + 1);
  display.print("0 kg");
  display.setCursor(GRAPH_WIDTH - 25, GRAPH_HEIGHT + 1);
  display.print(maxWeightDetected, 1); // Mostra o valor máximo detectado como limite do eixo X
  display.setCursor(0, 0);
  display.print("3.3V"); // Valor máximo do eixo Y (3.3V)
  display.setCursor(0, 10);       // Posiciona Vout logo abaixo do 3.3
  display.print("Vout");          // Rótulo do eixo Y
  
  // Desenha a curva de resposta ajustada e preenche a área até a curva
  int fillWidth = map(weight, 0, maxWeightDetected, 0, GRAPH_WIDTH); // Mapeia o peso atual para a largura da tela
  for (int x = 0; x < GRAPH_WIDTH; x++) {
    float normalizedPos = (float)x / GRAPH_WIDTH;
    float curveY;
    
    // Ajuste da curva com base no fator
    if (curveFactor > 0) {
      curveY = pow(normalizedPos, 1.0 + curveFactor);
    } else if (curveFactor < 0) {
      curveY = 1 - pow(1 - normalizedPos, 1.0 - curveFactor);
    } else {
      curveY = normalizedPos;
    }

    int y = GRAPH_HEIGHT - (curveY * GRAPH_HEIGHT);
    display.drawPixel(x, y, WHITE);

    // Preenche a área inferior até a curva, somente até o valor atual
    if (x <= fillWidth) {
      display.drawLine(x, GRAPH_HEIGHT, x, y, WHITE);
    }
  }

  display.display();
  delay(100); // Atraso para atualização de leitura
}
