#include <Encoder.h>            // Biblioteca para o Rotary Encoder
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>   // Biblioteca para o display OLED

// Definições do OLED
#define OLED_RESET -1
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

// Definições do Rotary Encoder
#define ENCODER_CLK 9   // Pino CLK do encoder
#define ENCODER_DT 10   // Pino DT do encoder
Encoder encoder(ENCODER_CLK, ENCODER_DT); // Instância do encoder

long lastPosition = -999; // Última posição registrada
String direction = "Parado"; // Direção do movimento

void setup() {
  // Inicializa o OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Endereço padrão I2C
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Mensagem inicial no display
  display.setCursor(0, 0);
  display.println("Teste do Encoder");
  display.println("Gire para ajustar.");
  display.display();

  Serial.begin(9600);
}

void loop() {
  // Lê a posição atual do encoder
  long newPosition = encoder.read();

  // Detecta mudança de posição
  if (newPosition != lastPosition) {
    if (newPosition > lastPosition) {
      direction = "Horário"; // Sentido horário (CW)
    } else {
      direction = "Anti-Horário"; // Sentido anti-horário (CCW)
    }

    // Atualiza a última posição
    lastPosition = newPosition;

    // Atualiza o display OLED
    updateDisplay(newPosition, direction);

    // Apenas para monitoramento serial (opcional)
    Serial.print("Posicao: ");
    Serial.print(newPosition);
    Serial.print(" | Direcao: ");
    Serial.println(direction);
  }
}

// Atualiza os valores exibidos no OLED
void updateDisplay(long position, String direction) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Teste do Encoder");

  display.setCursor(0, 16);
  display.print("Posicao: ");
  display.print(position);

  display.setCursor(0, 32);
  display.print("Direcao: ");
  display.print(direction);

  display.display();
}
