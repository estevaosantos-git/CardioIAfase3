#include <DHT.h>

// ==========================================
// 1. CONFIGURAÇÕES DOS SENSORES
// ==========================================
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define BOTAO_PIN 4
int batimentos = 0;
bool estadoAnteriorBotao = HIGH;

unsigned long tempoAnteriorSensores = 0;
const long intervaloSensores = 2000; // Lê a temperatura a cada 2 segundos


// ==========================================
// 2. CONFIGURAÇÕES DE WI-FI E MEMÓRIA OFFLINE (NOVO)
// ==========================================
bool wifiConectado = true; // Variável que simula se temos internet ou não
unsigned long tempoAnteriorWiFi = 0;

// Nossa "caderneta" para guardar dados quando estiver sem internet
const int MAX_DADOS = 50; // Limite de 50 anotações para não travar a memória
String memoriaOffline[MAX_DADOS]; // O Array (lista) que guarda os textos
int quantidadeSalva = 0; // Contador de quantas anotações já fizemos


// ==========================================
// 3. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(BOTAO_PIN, INPUT_PULLUP);

  Serial.println("==================================");
  Serial.println("Sistema Medico Iniciado.");
  Serial.println("🌐 [REDE] Wi-Fi Conectado!");
  Serial.println("==================================");
}


// ==========================================
// 4. LOOP PRINCIPAL
// ==========================================
void loop() {
  unsigned long tempoAtual = millis();

  // --- A. SIMULADOR DE QUEDA DE WI-FI (A cada 10 segundos) ---
  if (tempoAtual - tempoAnteriorWiFi >= 10000) {
    tempoAnteriorWiFi = tempoAtual;
    wifiConectado = !wifiConectado; // Inverte o sinal (se estava com internet, agora fica sem)

    if (wifiConectado) {
      Serial.println("\n🌐 [REDE] Wi-Fi RESTABELECIDO!");

      // Sincronizando os dados que estavam guardados na caderneta
      if (quantidadeSalva > 0) {
        Serial.println("🔄 Sincronizando dados retidos...");
        for (int i = 0; i < quantidadeSalva; i++) {
          Serial.print("☁️ [ENVIADO PARA NUVEM]: ");
          Serial.println(memoriaOffline[i]);
        }
        quantidadeSalva = 0; // Apaga a caderneta após enviar tudo
        Serial.println("✅ Sincronizacao concluida. Memoria limpa.\n");
      }
    } else {
      Serial.println("\n❌ [REDE] Wi-Fi CAIU! Entrando em modo offline (Edge Computing).\n");
    }
  }


  // --- B. LENDO OS BATIMENTOS ---
  bool estadoAtualBotao = digitalRead(BOTAO_PIN);
  if (estadoAtualBotao == LOW && estadoAnteriorBotao == HIGH) {
    batimentos++;
    String dadoBatimento = "Batimento detectado! Total: " + String(batimentos);

    if (wifiConectado) {
      Serial.println("☁️ [NUVEM] " + dadoBatimento); // Envia direto
    } else {
      // Salva na caderneta se houver espaço
      if (quantidadeSalva < MAX_DADOS) {
        memoriaOffline[quantidadeSalva] = dadoBatimento;
        quantidadeSalva++;
        Serial.println("💾 [OFFLINE] Batimento salvo na memoria local.");
      } else {
        Serial.println("⚠️ [ERRO] Memoria cheia! Dado perdido.");
      }
    }
    delay(50);
  }
  estadoAnteriorBotao = estadoAtualBotao;


  // --- C. LENDO A TEMPERATURA ---
  if (tempoAtual - tempoAnteriorSensores >= intervaloSensores) {
    tempoAnteriorSensores = tempoAtual;
    float temperatura = dht.readTemperature();

    if (!isnan(temperatura)) {
      String dadoTemp = "Temperatura: " + String(temperatura) + " °C";

      if (wifiConectado) {
        Serial.println("☁️ [NUVEM] " + dadoTemp); // Envia direto
      } else {
        // Salva na caderneta se houver espaço
        if (quantidadeSalva < MAX_DADOS) {
          memoriaOffline[quantidadeSalva] = dadoTemp;
          quantidadeSalva++;
          Serial.println("💾 [OFFLINE] Temperatura salva na memoria local.");
        } else {
          Serial.println("⚠️ [ERRO] Memoria cheia! Dado perdido.");
        }
      }
    }
  }
}