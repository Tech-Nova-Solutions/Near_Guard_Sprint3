#include <WiFi.h>
#include <PubSubClient.h>
#include <NewPing.h>
#include <LiquidCrystal_I2C.h>

// Configurações - variáveis editáveis
const char* default_SSID = "Wokwi-GUEST"; // Nome da rede Wi-Fi
const char* default_PASSWORD = ""; // Senha da rede Wi-Fi
const char* default_BROKER_MQTT = "52.137.83.133"; // IP do Broker MQTT
const int default_BROKER_PORT = 1883; // Porta do Broker MQTT
const char* default_TOPICO_SUBSCRIBE = "/TEF/proximitySensor001/cmd"; // Tópico MQTT de escuta
const char* default_TOPICO_PUBLISH_1 = "/TEF/proximitySensor001/attrs"; // Tópico MQTT de envio de informações para Broker
const char* default_TOPICO_PUBLISH_2 = "/TEF/proximitySensor001/attrs/d"; // Tópico MQTT de envio de informações para Broker
const char* default_ID_MQTT = "fiware_003"; // ID MQTT
const int default_D4 = 2; // Pino do LED onboard
const char* topicPrefix = "proximitySensor001"; // Prefixo do tópico

// Variáveis para configurações editáveis
char* SSID = const_cast<char*>(default_SSID);
char* PASSWORD = const_cast<char*>(default_PASSWORD);
char* BROKER_MQTT = const_cast<char*>(default_BROKER_MQTT);
int BROKER_PORT = default_BROKER_PORT;
char* TOPICO_SUBSCRIBE = const_cast<char*>(default_TOPICO_SUBSCRIBE);
char* TOPICO_PUBLISH_1 = const_cast<char*>(default_TOPICO_PUBLISH_1);
char* TOPICO_PUBLISH_2 = const_cast<char*>(default_TOPICO_PUBLISH_2);
char* ID_MQTT = const_cast<char*>(default_ID_MQTT);
int D4 = default_D4;

#define TRIGGER_PIN 5      // Pino do sensor de ultrassom
#define ECHO_PIN 18        // Pino do sensor de ultrassom
#define MAX_DISTANCE 430   // Distância máxima do sensor de proximidade (cm)

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

// Inicialização do display LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Endereço I2C: 0x27, 16 colunas, 2 linhas

WiFiClient espClient;
PubSubClient MQTT(espClient);
char EstadoSaida = '0';

float mediaD; // Variável para armazenar a média da distância

byte logoMahindra[6][8] = {
  {B00000, B10000, B11000, B10100, B10010, B10001, B10000, B10000},
  {B00000, B00000, B00000, B00000, B00000, B00000, B10001, B01010},
  {B00000, B00001, B00011, B00101, B01001, B10001, B00001, B00001},
  {B10000, B10000, B10000, B10000, B10000, B10000, B01111, B00000},
  {B01010, B01010, B01010, B01010, B01010, B10001, B00000, B00000},
  {B00001, B00001, B00001, B00001, B00001, B00001, B11110, B00000}
};

void initSerial() {
    Serial.begin(115200);
}

void initWiFi() {
    delay(10);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
    reconectWiFi();
}

void initMQTT() {
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
    MQTT.setCallback(mqtt_callback);
}

void setup() {
    initSerial();
    initWiFi();
    initMQTT();
    lcd.init();          // Inicializa o display LCD
    lcd.backlight();      // Ativa a luz de fundo do display LCD
    for (int i = 0; i < 6; i++) {
    lcd.createChar(i, logoMahindra[i]);
    }
    mostrarInicio();      // Exibe a mensagem inicial no LCD
    delay(5000);
    MQTT.publish(TOPICO_PUBLISH_1, "s|on");
}

void loop() {
    VerificaConexoesWiFIEMQTT();
    
    LerVariaveis();      // Mede a distância e calcula a média
    mostrar();           // Exibe a distância no display LCD
    handleDistance();    // Envia a distância medida para o broker MQTT
    MQTT.loop();
}

void reconectWiFi() {
    if (WiFi.status() == WL_CONNECTED)
        return;
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());

    // Garantir que o LED inicie desligado
    digitalWrite(D4, LOW);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (int i = 0; i < length; i++) {
        char c = (char)payload[i];
        msg += c;
    }
    Serial.print("- Mensagem recebida: ");
    Serial.println(msg);

    // Forma o padrão de tópico para comparação
    String onTopic = String(topicPrefix) + "@on|";
    String offTopic = String(topicPrefix) + "@off|";

    // Compara com o tópico recebido
    if (msg.equals(onTopic)) {
        digitalWrite(D4, HIGH);
        EstadoSaida = '1';
    }

    if (msg.equals(offTopic)) {
        digitalWrite(D4, LOW);
        EstadoSaida = '0';
    }
}

void VerificaConexoesWiFIEMQTT() {
    if (!MQTT.connected())
        reconnectMQTT();
    reconectWiFi();
}


void reconnectMQTT() {
    while (!MQTT.connected()) {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            MQTT.subscribe(TOPICO_SUBSCRIBE);
        } else {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Haverá nova tentativa de conexão em 2s");
            delay(2000);
        }
    }
}

// Função handleDistance para medir a distância e enviar ao broker
void handleDistance() {
    String mensagem = String(mediaD);  // Converte a média de metros para centímetros
    Serial.print("Distância medida: ");
    Serial.println(mensagem.c_str());
    MQTT.publish(TOPICO_PUBLISH_2, mensagem.c_str()); // Publica a distância no tópico MQTT
}

// Função para calcular a média da distância medida
void LerVariaveis() {
    float distancia = 0;
    float S[5];

    for (int i = 0; i < 5; i++) {
        S[i] = sonar.ping_cm();
        distancia += S[i];

    }

    mediaD = distancia / 5;
    mediaD /= 100;  // Converte para metros
    

}


// Função para exibir a distância no display LCD
void mostrar() {
    lcd.clear();
    if (mediaD <= 1) {
        lcd.setCursor(0, 0);
        lcd.print(mediaD);
        lcd.setCursor(4, 0);
        lcd.print("m");
        lcd.setCursor(0, 1);
        lcd.print("MUITO PERTO!");
    } else {
        lcd.setCursor(0, 0);
        lcd.print(mediaD);
        lcd.setCursor(4, 0);
        lcd.print("m");
        lcd.setCursor(0, 1);
        lcd.print("BOA DISTANCIA!");
    }
}

// Função para mostrar a mensagem inicial no display
void mostrarInicio() {
    lcd.clear();

    for (int i = 0; i < 8; i++) {
        lcd.setCursor(i, 0);
        lcd.print("Mahindra"[i]);
        delay(100);
    }

    for (int i = 0; i < 4; i++) {
        lcd.setCursor(4 + i, 1);
        lcd.print("Race"[i]);
        delay(100);
    }

    lcd.setCursor(13, 0);
    lcd.write(byte(0));
    delay(100);
    lcd.setCursor(14, 0);
    lcd.write(byte(1));
    delay(100);
    lcd.setCursor(15, 0);
    lcd.write(byte(2));
    delay(100);
    lcd.setCursor(13, 1);
    lcd.write(byte(3));
    delay(100);
    lcd.setCursor(14, 1);
    lcd.write(byte(4));
    delay(100);
    lcd.setCursor(15, 1);
    lcd.write(byte(5));

    delay(500);
}
