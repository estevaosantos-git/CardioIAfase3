#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <esp_system.h>

// Mesmo circuito da Parte 1. O botão fecha o GPIO 4 contra o GND.
constexpr int BOTAO = 4;
constexpr uint32_t JANELA_MS = 15000;
constexpr char TOPICO[] = "cardioia/lab-478271631471196161/v1/sinais";
DHT dht(15, DHT22);
WiFiClient rede;
PubSubClient mqtt(rede);

struct Amostra {
  uint32_t seq, tempo, janela, pulsos;
  float temperatura;
};
QueueHandle_t fila;
char sessao[16];

// Esta tarefa continua medindo enquanto a outra tenta se conectar ao broker.
void coletar(void*) {
  uint32_t inicio = millis(), alterado = inicio, pulsos = 0, seq = 0;
  int bruto = HIGH, estavel = HIGH;
  for (;;) {
    uint32_t agora = millis();
    int leitura = digitalRead(BOTAO);
    if (leitura != bruto) {
      bruto = leitura;
      alterado = agora;
    }
    if (bruto != estavel && agora - alterado >= 35) {
      estavel = bruto;
      if (estavel == LOW) ++pulsos;
    }
    uint32_t duracao = agora - inicio;
    if (duracao >= JANELA_MS) {
      Amostra a = {++seq, agora, duracao, pulsos, dht.readTemperature()};
      // Até 120 janelas (cerca de 30 minutos). Fila cheia preserva as anteriores.
      if (xQueueSend(fila, &a, 0) != pdTRUE)
        Serial.printf("Fila cheia: amostra %lu descartada\n", (unsigned long)a.seq);
      inicio = agora;
      pulsos = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOTAO, INPUT_PULLUP);
  dht.begin();
  snprintf(sessao, sizeof(sessao), "%08lx", (unsigned long)esp_random());
  fila = xQueueCreate(120, sizeof(Amostra));
  if (!fila || xTaskCreate(coletar, "coleta", 4096, nullptr, 1, nullptr) != pdPASS) {
    Serial.println("Falha ao iniciar a coleta");
    for (;;) delay(1000);
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin("Wokwi-GUEST", "", 6);
  mqtt.setServer("broker.hivemq.com", 1883);
  mqtt.setSocketTimeout(3);
  mqtt.setBufferSize(512);
  Serial.println("Coleta iniciada: uma amostra a cada 15 segundos.");
}

void loop() {
  static uint32_t tentativa = 0;
  if (WiFi.status() != WL_CONNECTED) { delay(20); return; }
  if (!mqtt.connected()) {
    if (millis() - tentativa >= 3000) {
      tentativa = millis();
      char cliente[32];
      snprintf(cliente, sizeof(cliente), "cardioia-%s", sessao);
      if (mqtt.connect(cliente)) Serial.println("MQTT conectado");
      else Serial.printf("MQTT indisponivel: %d\n", mqtt.state());
    }
    delay(10);
    return;
  }
  mqtt.loop();
  Amostra a;
  if (xQueuePeek(fila, &a, 0) == pdTRUE) {
    char temp[16] = "null", mensagem[256];
    if (isfinite(a.temperatura)) snprintf(temp, sizeof(temp), "%.1f", a.temperatura);
    // A idade permite reconstruir a ordem temporal de registros reenviados.
    snprintf(mensagem, sizeof(mensagem),
      "{\"sessao\":\"%s\",\"seq\":%lu,\"idade_ms\":%lu,\"bpm\":%ld,\"temp_c\":%s}",
      sessao, (unsigned long)a.seq, (unsigned long)(millis() - a.tempo),
      lround(60000.0 * a.pulsos / a.janela), temp);
    if (mqtt.publish(TOPICO, mensagem, false)) {
      xQueueReceive(fila, &a, 0);
      Serial.println(mensagem);
    }
  }
  delay(10);
}
