/*
 * ============================================================
 *  Station IoT ESP32 — Architecture multi-tâches FreeRTOS
 * ============================================================
 *  Auteur       : [ons garés]
 *  Description  : Firmware IoT publiant température/humidité
 *                 via MQTT, avec architecture FreeRTOS propre :
 *                 découplage capteur / réseau via une queue,
 *                 gestion robuste de la reconnexion Wi-Fi/MQTT.
 *
 *  Matériel     : ESP32 DevKit + capteur DHT22
 *  Simulation   : https://wokwi.com (voir diagram.json)
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ---------------------------------------------------------------
// Configuration réseau et MQTT
// ---------------------------------------------------------------
const char* WIFI_SSID     = "Wokwi-GUEST";   // Réseau simulé Wokwi
const char* WIFI_PASSWORD = "";

const char* MQTT_SERVER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "tunisie/iot/station_meteo";
const char* MQTT_CLIENT_PREFIX = "ESP32-Station-";

// ---------------------------------------------------------------
// Configuration capteur
// ---------------------------------------------------------------
#define DHT_PIN  15
#define DHT_TYPE DHT22
#define SENSOR_READ_INTERVAL_MS  5000
#define PUBLISH_INTERVAL_MS      5000

// ---------------------------------------------------------------
// Objets globaux
// ---------------------------------------------------------------
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
DHT          dhtSensor(DHT_PIN, DHT_TYPE);

// Structure de données échangée entre tâches
struct SensorData {
  float temperature;
  float humidity;
  bool  valid;
};

QueueHandle_t sensorDataQueue;

// ---------------------------------------------------------------
// Tâche 1 : lecture du capteur (Core 0)
// Rôle unique : lire le DHT22 et publier la valeur dans la queue.
// Ne touche jamais au réseau -> aucune dépendance croisée.
// ---------------------------------------------------------------
void SensorTask(void* pvParameters) {
  dhtSensor.begin();
  SensorData data;

  for (;;) {
    data.temperature = dhtSensor.readTemperature();
    data.humidity    = dhtSensor.readHumidity();
    data.valid = !isnan(data.temperature) && !isnan(data.humidity);

    if (data.valid) {
      // xQueueOverwrite : la queue ne garde que la dernière valeur.
      // La tâche réseau consomme toujours la donnée la plus récente,
      // sans jamais bloquer la tâche capteur si elle est en retard.
      xQueueOverwrite(sensorDataQueue, &data);
      Serial.printf("[SensorTask]  T=%.1f°C  H=%.1f%%\n", data.temperature, data.humidity);
    } else {
      Serial.println("[SensorTask]  Erreur de lecture DHT22");
    }

    vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
  }
}

// ---------------------------------------------------------------
// Fonctions utilitaires réseau
// ---------------------------------------------------------------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("[NetworkTask] Connexion Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(" connecté (IP: %s)\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(" échec (timeout) — nouvelle tentative au prochain cycle");
  }
}

void connectMQTT() {
  if (mqttClient.connected()) return;

  String clientId = MQTT_CLIENT_PREFIX + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.printf("[NetworkTask] Connexion MQTT (%s)...", clientId.c_str());

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println(" connecté");
  } else {
    Serial.printf(" échec (rc=%d)\n", mqttClient.state());
  }
}

// ---------------------------------------------------------------
// Tâche 2 : réseau + publication MQTT (Core 1)
// Rôle unique : maintenir Wi-Fi/MQTT actifs et publier la dernière
// donnée disponible dans la queue. Ne lit jamais le capteur.
// ---------------------------------------------------------------
void NetworkTask(void* pvParameters) {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  SensorData data;

  for (;;) {
    connectWiFi();
    connectMQTT();
    mqttClient.loop();

    if (mqttClient.connected() &&
        xQueuePeek(sensorDataQueue, &data, pdMS_TO_TICKS(100)) == pdTRUE &&
        data.valid) {

      char payload[64];
      snprintf(payload, sizeof(payload),
               "{\"temperature\":%.1f,\"humidity\":%.1f}",
               data.temperature, data.humidity);

      if (mqttClient.publish(MQTT_TOPIC, payload)) {
        Serial.printf("[NetworkTask] Publié -> %s : %s\n", MQTT_TOPIC, payload);
      } else {
        Serial.println("[NetworkTask] Échec de publication");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
  }
}

// ---------------------------------------------------------------
// Setup : création de la queue et des deux tâches FreeRTOS,
// chacune épinglée sur un cœur distinct de l'ESP32 (dual-core).
// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Démarrage station IoT ESP32 (FreeRTOS) ===");

  sensorDataQueue = xQueueCreate(1, sizeof(SensorData));

  xTaskCreatePinnedToCore(
    SensorTask, "SensorTask",
    4096, NULL, 1, NULL,
    0   // Core 0
  );

  xTaskCreatePinnedToCore(
    NetworkTask, "NetworkTask",
    4096, NULL, 1, NULL,
    1   // Core 1
  );
}

// loop() n'est pas utilisée : tout le travail vit dans les tâches FreeRTOS.
void loop() {
  vTaskDelete(NULL);
}
