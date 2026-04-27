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
  float curveShape;
};
FlashStorage(flashConfig, Config); // Aloca espaço na flash

// Configuração inicial
float maxVoltage = 3.3;           // Tensão máxima inicial
float curveFactor = 0.0;          // Inclinação inicial
float curveShape = 0.0;          // Formato inicial da curva
int lastEncoderPosition = 0;
bool buttonPressed = false;
int configMode = 0; // 0 = Vmax, 1 = Inclinação, 2 = Formato

// Configuração do HX711
const float CALIBRATION_FACTOR = -44.765419; // Fator de calibração
const long OFFSET = 1877599;                 // Offset para zero
float pressure = 0;                          // Pressão medida em kg

// Dados do gráfico
const int GRAPH_WIDTH = 122;      // Largura do gráfico
const int GRAPH_HEIGHT = 64;      // Altura do gráfico
const float MIN_VOLTAGE = 1.47;   // Tensão mínima fixa para o MCP4725

float previousPressure = 0;       // Última tensão registrada para sincronização

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
  // Leitura do estado do botão do encoder
  bool currentButtonPressed = digitalRead(ROTARY_SW_PIN) == LOW;
  if (currentButtonPressed && !buttonPressed) {
    configMode = (configMode + 1) % 3; // Alterna entre Vmax, Inclinação e Formato
    saveSettings(); // Salva as configurações na flash
    if (configMode == 0)
      Serial.println("Ajustando Vmax");
    else if (configMode == 1)
      Serial.println("Ajustando Inclinação");
    else if (configMode == 2)
      Serial.println("Ajustando Formato");
  }
  buttonPressed = currentButtonPressed;

  // Leitura do encoder rotativo
  int newEncoderPosition = rotaryEncoder.read() / 2; // Divide por 2 para maior controle
  if (newEncoderPosition != lastEncoderPosition) {
    int delta = newEncoderPosition - lastEncoderPosition;

    if (configMode == 0) {
      maxVoltage += delta * 0.05; // Incrementa ou decrementa em passos de 0.05
      maxVoltage = constrain(maxVoltage, 2.5, 3.3);
    } else if (configMode == 1) {
      curveFactor += delta * 0.01; // Incrementa ou decrementa em passos de 0.01
      curveFactor = constrain(curveFactor, -1.0, 1.0);
    } else if (configMode == 2) {
      curveShape += delta * 0.01; // Incrementa ou decrementa em passos de 0.01
      curveShape = constrain(curveShape, -1.0, 1.0);
    }

    lastEncoderPosition = newEncoderPosition;
  }

  // Leitura da pressão do pedal
  if (scale.is_ready()) {
    pressure = scale.get_units() / 1000.0; // Converte para kg
    pressure = constrain(pressure, 0.0, 50.0); // Limita a pressão entre 0 e 50 kg

    // Atualiza a saída do DAC
    updateDAC();

    // Atualiza o display sincronizado com a leitura
    updateDisplay();
  }
}

void updateDAC() {
  // Normaliza a pressão (0 a 1)
  float normalizedPressure = pressure / 50.0; // Pressão máxima de 50 kg
  normalizedPressure = constrain(normalizedPressure, 0.0, 1.0);

  // Aplica o formato da curva
  float adjustedPressure = normalizedPressure + curveShape * normalizedPressure * (1.0 - normalizedPressure);

  // Aplica o ajuste de inclinação
  adjustedPressure *= (1.0 + curveFactor);

  // Garante que o valor esteja dentro dos limites
  adjustedPressure = constrain(adjustedPressure, 0.0, 1.0);

  // Calcula a tensão de saída invertida
  float outputVoltage = MIN_VOLTAGE + (1.0 - adjustedPressure) * (maxVoltage - MIN_VOLTAGE);

  // Converte para 12 bits para enviar ao DAC
  uint16_t dacOutput = (uint16_t)((outputVoltage / 3.3) * 4095);

  // Atualiza o DAC
  dac.setVoltage(dacOutput, false);

  // Atualiza a tensão exibida no display
  previousPressure = outputVoltage; // Salva o valor atual da tensão
}

void updateDisplay() {
  display.clearDisplay();

  // Desenha o gráfico
  drawGraph();

  // Exibe o valor de pressão no canto superior esquerdo
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(WHITE, BLACK);
  display.print(pressure, 0);
  display.print("kg");

  // Exibe o valor atual de configuração
  display.setTextSize(1);
  display.setCursor(0, 20);
  if (configMode == 0) {
    display.print("Vmax:");
    display.print(maxVoltage, 2);
    display.print("V");
  } else if (configMode == 1) {
    display.print("I:");
    display.print(curveFactor, 2);
  } else if (configMode == 2) {
    display.print("S:");
    display.print(curveShape, 2);
  }

  // Exibe a tensão no canto inferior direito
  display.setTextSize(2);
  String voltageText = String(previousPressure, 2) + "V";
  int16_t x, y;
  uint16_t width, height;
  display.getTextBounds(voltageText, 0, 0, &x, &y, &width, &height);
  display.setCursor(128 - width, 64 - height);
  display.print(voltageText);

  display.display();
}

void drawGraph() {
  for (int x = 0; x < GRAPH_WIDTH; x++) {
    // Posição normalizada no eixo X (0 a 1)
    float rawPos = (float)x / GRAPH_WIDTH;
    float curveY;

    // Aplica o formato da curva mantendo os extremos fixos
    if (curveShape > 0) {
      curveY = pow(rawPos, 1 + curveShape);
    } else if (curveShape < 0) {
      curveY = 1 - pow(1 - rawPos, 1 - curveShape);
    } else {
      curveY = rawPos; // Forma linear
    }

    // Ajusta a inclinação da curva
    curveY *= (1.0 + curveFactor);

    // Mapeia o valor de curva para o intervalo de tensão real
    float voltage = MIN_VOLTAGE + curveY * (maxVoltage - MIN_VOLTAGE);

    // Normaliza para o range do gráfico (0 a 1)
    float normalizedVoltage = (voltage - MIN_VOLTAGE) / (maxVoltage - MIN_VOLTAGE);

    // Converte para coordenadas do gráfico
    int y = constrain(GRAPH_HEIGHT - (normalizedVoltage * GRAPH_HEIGHT), 0, GRAPH_HEIGHT - 1);

    // Desenha o gráfico
    display.drawPixel(x, y, WHITE);

    // Preenche até o nível correspondente à pressão atual
    if (rawPos <= pressure / 50.0) {
      display.drawLine(x, GRAPH_HEIGHT - 1, x, y, WHITE);
    }
  }
}

void saveSettings() {
  Config config = {maxVoltage, curveFactor, curveShape};
  flashConfig.write(config);
  Serial.println("Configurações salvas na memória flash.");
}

void loadSettings() {
  Config config = flashConfig.read();
  maxVoltage = config.maxVoltage;
  curveFactor = config.curveFactor;
  curveShape = config.curveShape;

  if (maxVoltage < 2.5 || maxVoltage > 3.3) {
    maxVoltage = 3.3;
  }
  if (curveFactor < -1.0 || curveFactor > 1.0) {
    curveFactor = 0.0;
  }
  if (curveShape < -1.0 || curveShape > 1.0) {
    curveShape = 0.0;
  }

  Serial.print("Configurações carregadas: Vmax=");
  Serial.print(maxVoltage, 2);
  Serial.print("V, Inclinação=");
  Serial.print(curveFactor, 2);
  Serial.print(", Formato=");
  Serial.println(curveShape, 2);
}
