#include <HX711.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Configuração de pinos
#define DT_PIN 7
#define SCK_PIN 6
#define OLED_RESET -1

// Objetos
HX711 scale;
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

// Variáveis para calibração
float calibrationFactor = 1.0; // Fator inicial arbitrário
long offset = 0;               // Offset inicial
const float knownWeight = 5.0; // Peso conhecido em libras (5lb)

// Controle de estado da calibração
enum CalibrationStep {
  SET_OFFSET,
  CALIBRATE_SCALE,
  FINISHED
};
CalibrationStep currentStep = SET_OFFSET;

int calibrationRound = 0; // Contador de rodadas de calibração
float calibrationResults[3]; // Resultados intermediários do fator de calibração
float finalCalibrationFactor = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando Calibracao Automatica do HX711...");

  scale.begin(DT_PIN, SCK_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.display();

  Serial.println("Conecte o HX711 e aguarde...");
}

void loop() {
  if (!scale.is_ready()) {
    Serial.println("HX711 nao esta pronto. Verifique conexoes.");
    delay(1000);
    return;
  }

  switch (currentStep) {
    case SET_OFFSET:
      calibrateOffset();
      break;

    case CALIBRATE_SCALE:
      calibrateScale();
      break;

    case FINISHED:
      displayLoad();
      break;
  }
}

void calibrateOffset() {
  Serial.println("Calibrando Offset... Remova todos os pesos da celula de carga.");
  displayMessage("Calibrando Offset", "Remova os pesos!");
  delay(5000); // Tempo para o usuário remover pesos

  scale.tare(); // Define o offset automaticamente
  offset = scale.get_offset();
  Serial.print("Offset calculado: ");
  Serial.println(offset);

  currentStep = CALIBRATE_SCALE;
}

void calibrateScale() {
  Serial.print("Rodada de Calibracao ");
  Serial.println(calibrationRound + 1);
  displayMessage("Calibracao", "Coloque 5lb");

  delay(5000); // Tempo para o usuário colocar o peso

  float reading = scale.get_units(10); // Lê a média de 10 leituras
  float factor = reading / knownWeight; // Ajusta o fator de escala
  calibrationResults[calibrationRound] = factor;

  Serial.print("Peso lido: ");
  Serial.println(reading, 2);
  Serial.print("Fator de Calibracao calculado: ");
  Serial.println(factor, 6);

  calibrationRound++;

  if (calibrationRound < 3) {
    Serial.println("Repita o processo de calibracao.");
    displayMessage("Repita o Processo", "Calibracao 5lb");
    delay(2000);
  } else {
    // Calcula a média dos fatores de calibração
    finalCalibrationFactor = (calibrationResults[0] + calibrationResults[1] + calibrationResults[2]) / 3.0;
    scale.set_scale(finalCalibrationFactor);

    Serial.println("Calibracao concluida!");
    Serial.print("Offset final: ");
    Serial.println(offset);
    Serial.print("Fator de Calibracao final (media): ");
    Serial.println(finalCalibrationFactor, 6);

    displayMessage("Calibracao Final", "Pronto para medir!");
    currentStep = FINISHED;
  }
}

void displayLoad() {
  float weight = scale.get_units(10); // Lê a média de 10 leituras

  // Atualiza o display OLED com a carga atual
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Peso:");
  display.setCursor(0, 30);
  display.print(weight, 2);
  display.print(" lb");

  display.display();

  // Exibe na serial também
  Serial.print("Peso atual: ");
  Serial.println(weight, 2);

  delay(500); // Atualiza o display a cada 500ms
}

void displayMessage(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 20);
  display.println(line2);
  display.display();
}
