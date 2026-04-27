#include <HX711.h>

// Definições dos pinos
#define DT_PIN 3       // Pino DT do HX711 no pino digital 3 do Arduino Nano
#define SCK_PIN 4      // Pino SCK do HX711 no pino digital 4 do Arduino Nano

HX711 scale;

void setup() {
  Serial.begin(9600);
  scale.begin(DT_PIN, SCK_PIN);
  Serial.println("Iniciando leitura do HX711...");
}

void loop() {
  // Lê o valor bruto (RAW) do HX711
  long rawValue = scale.read();
  Serial.print("RAW: ");
  Serial.println(rawValue);

  delay(500);  // Atualiza a leitura a cada 500 ms
}
