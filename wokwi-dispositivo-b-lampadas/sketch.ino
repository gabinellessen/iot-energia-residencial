/*
 * Sistema Inteligente de Gestao Energetica Residencial com IoT
 *
 * Dispositivo de circuito: mede a corrente no quadro de luz e aciona o rele
 * do seu circuito. Mesmo codigo nos dois dispositivos (A - tomadas e
 * B - lampadas); a unica diferenca e a constante CIRCUITO logo abaixo.
 *
 * Topicos MQTT (substituindo <circuito> pelo valor da constante):
 *   iot/residencial/<circuito>/consumo   (publica  - Watts)
 *   iot/residencial/<circuito>/estado    (publica  - "ON" / "OFF", retained)
 *   iot/residencial/<circuito>/comando   (assina   - "ON" / "OFF")
 *
 * Grupo: Jean Alex da Silva, Deborah Jamilly de Abreu Souza,
 *        Gabriela Nellessen de Sousa
 * Disciplina: Objetos Inteligentes Conectados - Mackenzie 2026.02
 */

#include <WiFi.h>
#include <PubSubClient.h>

// =====================================================================
// CONFIGURACAO POR CIRCUITO (UNICA DIFERENCA ENTRE OS DOIS DISPOSITIVOS)
// =====================================================================
// Dispositivo A: CIRCUITO = "tomadas"
// Dispositivo B: CIRCUITO = "lampadas"
const char* CIRCUITO = "lampadas";

// ----- Hardware -----
#define POT_PIN    34   // GPIO34 (ADC) - potenciometro que emula SCT-013
#define RELAY_PIN  16   // GPIO16       - entrada IN do modulo rele

// ----- Wi-Fi (Wokwi usa SSID publico, sem senha) -----
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ----- Broker MQTT publico -----
const char* MQTT_HOST     = "broker.hivemq.com";
const uint16_t MQTT_PORT  = 1883;

// ----- Calibracao do sensor de corrente -----
// Potenciometro emula um SCT-013 com fundo de escala de 60 A,
// cobrindo a faixa tipica de circuitos do quadro residencial (incluindo
// disjuntores de chuveiro e ar-condicionado). Tensao nominal da rede
// adotada: 127 V (padrao paulista).
const float TENSAO_REDE_V  = 127.0;
const float CORRENTE_MAX_A = 60.0;

// ----- Intervalo de publicacao -----
const unsigned long INTERVALO_MS = 5000;

// ===== Variaveis derivadas =====
char CLIENT_ID[64];
char TOPIC_CONSUMO[64];
char TOPIC_ESTADO[64];
char TOPIC_COMANDO[64];
bool releLigado = true;

WiFiClient    espClient;
PubSubClient  mqtt(espClient);
unsigned long ultimaPublicacao = 0;


void montarTopicos() {
  snprintf(CLIENT_ID,     sizeof(CLIENT_ID),     "iot-residencial-%s", CIRCUITO);
  snprintf(TOPIC_CONSUMO, sizeof(TOPIC_CONSUMO), "iot/residencial/%s/consumo", CIRCUITO);
  snprintf(TOPIC_ESTADO,  sizeof(TOPIC_ESTADO),  "iot/residencial/%s/estado",  CIRCUITO);
  snprintf(TOPIC_COMANDO, sizeof(TOPIC_COMANDO), "iot/residencial/%s/comando", CIRCUITO);
}

void publicarEstado() {
  mqtt.publish(TOPIC_ESTADO, releLigado ? "ON" : "OFF", true);
}

void aplicarComando(const String& msg) {
  bool novo;
  if (msg == "ON"  || msg == "on"  || msg == "1" || msg == "true")  novo = true;
  else if (msg == "OFF" || msg == "off" || msg == "0" || msg == "false") novo = false;
  else {
    Serial.printf("[CMD] payload invalido: %s\n", msg.c_str());
    return;
  }
  releLigado = novo;
  digitalWrite(RELAY_PIN, releLigado ? HIGH : LOW);
  publicarEstado();
  Serial.printf("[CMD] circuito %s -> %s\n", CIRCUITO, releLigado ? "ON" : "OFF");
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("[RX] %s = %s\n", topic, msg.c_str());
  aplicarComando(msg);
}

void conectarWifi() {
  Serial.printf("\n[Wi-Fi] Conectando a %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print('.');
  }
  Serial.printf("\n[Wi-Fi] Conectado, IP: %s\n",
                WiFi.localIP().toString().c_str());
}

void conectarMqtt() {
  while (!mqtt.connected()) {
    Serial.printf("[MQTT] Conectando a %s ... ", MQTT_HOST);
    if (mqtt.connect(CLIENT_ID)) {
      Serial.println("OK");
      mqtt.subscribe(TOPIC_COMANDO);
      Serial.printf("[MQTT] assinado: %s\n", TOPIC_COMANDO);
      publicarEstado();  // estado inicial retained
    } else {
      Serial.printf("FALHA (rc=%d). Nova tentativa em 2s.\n", mqtt.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== Dispositivo (circuito %s) - inicializando ===\n", CIRCUITO);

  montarTopicos();

  pinMode(POT_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, releLigado ? HIGH : LOW);

  conectarWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqtt_callback);
}

void loop() {
  if (!mqtt.connected()) conectarMqtt();
  mqtt.loop();

  unsigned long agora = millis();
  if (agora - ultimaPublicacao < INTERVALO_MS) return;
  ultimaPublicacao = agora;

  // Quando o rele esta desligado o circuito fica aberto e nao ha corrente.
  float potenciaW = 0.0;
  float correnteA = 0.0;
  if (releLigado) {
    int   leituraAdc   = analogRead(POT_PIN);
    float fracaoEscala = leituraAdc / 4095.0;           // 0..1
    correnteA  = fracaoEscala * CORRENTE_MAX_A;         // 0..60 A
    potenciaW  = correnteA * TENSAO_REDE_V;             // 0..7620 W
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f", potenciaW);
  mqtt.publish(TOPIC_CONSUMO, buf);

  Serial.printf("[PUB] %s -> %.1f W (corrente %.2f A, rele=%s)\n",
                CIRCUITO, potenciaW, correnteA, releLigado ? "ON" : "OFF");
}
