#include <HX711.h>

#define DT_PIN 7
#define SCK_PIN 6

HX711 scale;

void setup() {
  Serial.begin(115200);
  Serial.println("Teste HX711 - Contagem e Frequência em Hz");
  scale.begin(DT_PIN, SCK_PIN);

  if (!scale.is_ready()) {
    Serial.println("HX711 não está pronto no setup. Verifique conexões.");
  } else {
    Serial.println("HX711 está pronto. Iniciando leituras...");
  }
}

void loop() {
  static unsigned long startTime = millis();
  static int validReadings = 0;

  // Tempo total para teste (10 segundos)
  if (millis() - startTime <= 10000) {
    if (scale.is_ready()) {
      scale.get_units(); // Faz a leitura
      validReadings++;   // Conta as leituras válidas
    }
  } else {
    // Calcula a frequência em Hz
    float frequency = validReadings / 10.0;

    // Exibe o número de leituras e a frequência
    Serial.print("Leituras válidas em 10 segundos: ");
    Serial.print(validReadings);
    Serial.print(" | Frequência: ");
    Serial.print(frequency, 2); // Exibe com duas casas decimais
    Serial.println(" Hz");

    // Reseta para nova contagem
    startTime = millis();
    validReadings = 0;
  }
}
