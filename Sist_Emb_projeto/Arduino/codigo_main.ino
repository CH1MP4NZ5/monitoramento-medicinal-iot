#include <WiFi.h>
#include <WiFiClientSecure.h>   // <-- ESSENCIAL para usar CloudAMQP (TLS)
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <PubSubClient.h>

// ======== CONFIGURAÇÕES DE PINOS ========
#define LED_WIFI 4 
#define DHTPIN 5
#define DHTTYPE DHT22

// ======== OBJETOS ========
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======== VARIÁVEIS ========
float temperatura = 0.0;
float umidade = 0.0;
unsigned long ultimoUpdate = 0;

// ======== CONFIG WI-FI ========
const char* ssid = "12345678";
const char* password = "abcd2945";

// ======== CONFIG MQTT (CloudAMQP) ========
const char* mqtt_server = "jaragua-01.lmq.cloudamqp.com";  // HOST 
const int mqtt_port = 8883;
const char* mqtt_user = "refqurtq:refqurtq";               // USUARIO 
const char* mqtt_pass = "JTeV6YUekaRgi6G0lbp7XNA_i3c71jqO";  // SENHA MQTT
   

WiFiClientSecure espClient;     // Cliente TLS
PubSubClient client(espClient); // Cliente MQTT

// ======== CALLBACK ========
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("Comando recebido: ");
  Serial.println(msg);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CMD:");
  lcd.print(msg);

  if (msg == "ligar") {
    Serial.println("Climatizador LIGADO");
  }
  else if (msg == "desligar") {
    Serial.println("Climatizador DESLIGADO");
  }
  else if (msg.startsWith("set_temp:")) {
    String valor = msg.substring(9);
    Serial.print("Temp alvo = ");
    Serial.println(valor);
  }
}

// ======== CONECTAR AO MQTT ========
void conectarMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT... ");

    if (client.connect("ESP32Lucas", mqtt_user, mqtt_pass)) {
      Serial.println("Conectado!");
      client.subscribe("climatizador/comando");
    } else {
      Serial.print("Erro = ");
      Serial.print(client.state());
      Serial.println(" | Tentando de novo em 5s...");
      delay(5000);
    }
  }
}

// ======== CONECTAR AO WI-FI ========
void conectarWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
    delay(500);
    Serial.print(".");
    if (millis() - inicio > 15000) {
      Serial.println("\nFalha na conexao WiFi!");
      lcd.clear();
      lcd.print("WiFi Falhou!");
      digitalWrite(LED_WIFI, LOW);
      return;
    }
  }

  digitalWrite(LED_WIFI, HIGH);
  Serial.println("\nWiFi Conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Conectado!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(2000);
  lcd.clear();
}

// ======== SETUP ========
void setup() {
  Serial.begin(115200);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, LOW);

  dht.begin();
  lcd.init();
  lcd.backlight();

  conectarWiFi();

  // MQTT
  espClient.setInsecure(); // Ignora certificado TLS (necessário CloudAMQP)
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

// ======== LOOP ========
void loop() {
  if (!client.connected()) conectarMQTT();
  client.loop();

  if (millis() - ultimoUpdate >= 2000) {
    ultimoUpdate = millis();

    temperatura = dht.readTemperature();
    umidade = dht.readHumidity();

    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("Falha ao ler DHT!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Erro no Sensor!");
      return;
    }

    Serial.print("Temp: ");
    Serial.print(temperatura);
    Serial.print(" | Umidade: ");
    Serial.println(umidade);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    lcd.print(temperatura, 1);
    lcd.print((char)223);
    lcd.print("C  ");
    lcd.print("U: ");
    lcd.print(umidade, 0);
    lcd.print("%");

    client.publish("climatizador/temperatura", String(temperatura).c_str());
    client.publish("climatizador/umidade", String(umidade).c_str());
  }
}
