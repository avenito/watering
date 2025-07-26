#include "WiFi.h"
#include "PubSubClient.h"
#include "DHT.h"

#define DHTPIN 0
#define DHTTYPE DHT11
#define timePulsePump 5000
#define timeToRead 0.5 * 60 * 1000

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "VIRUS2X";
const char* password = "60120036273529369257";
const int pump = 10;
const char* ntpServer1 = "ntp1.informatik.uni-erlangen.de";
const char* ntpServer2 = "ptbtime1.ptb.de";
const char* ntpServer3 = "ptbtime2.ptb.de";


//const char* mqtt_server = "maqiatto.com"; // IP do seu broker MQTT (ex: Mosquitto)
//const char* mqtt_user = "tissianorodri@gmail.com";  // se não tiver auth, deixe em branco
//const char* mqtt_pass = "vt260957";
const char* mqtt_server = "avenito.ddns.net";  // IP do seu broker MQTT (ex: Mosquitto)
const char* mqtt_user = "user01";              // se não tiver auth, deixe em branco
const char* mqtt_pass = "123456";
const int mqtt_port = 1883;

struct tm timeinfo;
int numReconn = -1;
char timeBuff[20];
char reconnNum[4];
String payloadStr;

boolean pumpOutput = false, lastPumpOutup = false;
static unsigned long timePumpOutput = 0;
static unsigned long timeToReadTH = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void printTimeStamp() {
  getLocalTime(&timeinfo);
  strftime(timeBuff, sizeof(timeBuff), "%d %b %y\n%H:%M:%S", &timeinfo);
  Serial.println(timeBuff);
}

void reconnect() {
  // Reconectar se desconectado
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect("User 01", mqtt_user, mqtt_pass)) {
      Serial.println("Conectado!");
      printTimeStamp();
      client.publish("st/conecTime", timeBuff);
      sprintf(reconnNum, "%d", ++numReconn);
      client.publish("st/reconnNum", reconnNum);
      client.subscribe("ctr/pump");  // inscreve-se em um tópico
    } else {
      Serial.print("Falhou, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  payloadStr = "";
  Serial.print("Mensagem recebida em: ");
  for (int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  Serial.print(topic);
  Serial.print(" => ");
  Serial.print(payloadStr);
  Serial.println();
  if (strcmp(topic, "ctr/pump") == 0) {
    if (payload[0] == '1') {
      pumpOutput = true;
    } else {
      pumpOutput = false;
    }
  } else {
    Serial.println("Topico nao correspondente!");
  }
  String cmd = String(payload[0]);
}

void setup() {
  pinMode(pump, OUTPUT);
  WiFi.begin(ssid, password);
  Serial.begin(115200);
  Serial.print("Conectando-se ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  configTime(2 * 3600, 1, ntpServer1, ntpServer2, ntpServer3);
  Serial.println("Sincronizando tempo...");
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Tempo sincronizado!");
  Serial.println(&timeinfo, "%d %b %y %H:%M:%S");
  dht.begin();
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Control pump output
  if (pumpOutput) {
    if (lastPumpOutup) {
      if (millis() - timePumpOutput > timePulsePump) {
        pumpOutput = false;
        lastPumpOutup = false;
        client.publish("ctr/pump", "0");
      }
    } else {
      lastPumpOutup = true;
      timePumpOutput = millis();
      printTimeStamp();
      client.publish("st/lastAct", timeBuff);
    }
  } else {
    lastPumpOutup = false;
  }
  if (pumpOutput) {
    digitalWrite(pump, LOW);
  } else {
    digitalWrite(pump, HIGH);
  }
  // END Control pump output

  // Tempo de leitura de temperatura e umidade
  if (millis() - timeToReadTH > timeToRead) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    char temp[5];
    char hum[5];
    sprintf(temp, "%.1f", t);
    sprintf(hum, "%.1f", h);
    client.publish("st/tempAmb", temp);
    client.publish("st/hum", hum);
    Serial.print("Humidity: ");
    Serial.print(hum);
    Serial.print("%  Temperature: ");
    Serial.print(temp);
    Serial.println("°C ");
    timeToReadTH = millis();
  }
}