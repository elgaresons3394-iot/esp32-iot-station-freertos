# Station IoT ESP32 — Architecture FreeRTOS multi-tâches

Firmware ESP32 publiant en temps réel des données de température et d'humidité
sur un broker MQTT, avec une architecture logicielle basée sur **FreeRTOS**
(deux tâches indépendantes, communication par queue, dual-core).

**Simulation en ligne (Wokwi)** : https://wokwi.com/projects/477064966556889089

---

## Pourquoi ce projet

Ce projet illustre une approche d'ingénierie embarquée plutôt qu'un simple
prototype :

- **Découplage capteur / réseau** : deux tâches FreeRTOS totalement
  indépendantes, communiquant via une `Queue`, plutôt qu'une seule boucle
  `loop()` qui mélange tout.
- **Dual-core** : la lecture capteur tourne sur le Core 0, le réseau sur le
  Core 1 (`xTaskCreatePinnedToCore`), exploitant l'architecture matérielle
  réelle de l'ESP32.
- **Robustesse réseau** : reconnexion Wi-Fi et MQTT automatique, avec timeout
  et retry, sans jamais bloquer la lecture du capteur.
- **Code de production** : pas de variables globales partagées sans
  protection, gestion des erreurs de lecture capteur, formatage JSON sans
  allocation dynamique inutile.

## Architecture

```
┌─────────────────┐        Queue         ┌──────────────────┐
│   SensorTask     │  ──────────────────▶ │   NetworkTask     │
│   (Core 0)       │   xQueueOverwrite    │   (Core 1)        │
│                   │                       │                    │
│  - Lit DHT22      │                       │  - Gère Wi-Fi      │
│  - Toutes les 5s  │                       │  - Gère MQTT       │
│                   │                       │  - Publie JSON     │
└─────────────────┘                       └──────────────────┘
```
## Matériel

| Composant     | Détail                          |
|---------------|----------------------------------|
| MCU           | ESP32 DevKit V1                  |
| Capteur       | DHT22 (température/humidité)     |
| Connexion     | GPIO 15 (data), 3V3, GND         |

Schéma complet dans [`diagram.json`](./diagram.json) (compatible Wokwi).

## Stack logicielle

- Framework Arduino (ESP32 core)
- FreeRTOS (natif sur ESP32)
- [PubSubClient](https://github.com/knolleary/pubsubclient) — client MQTT
- [DHT sensor library](https://github.com/adafruit/DHT-sensor-library) — Adafruit

## Configuration MQTT

| Paramètre | Valeur (démo)                          |
|-----------|-----------------------------------------|
| Broker    | `broker.hivemq.com` (broker public)      |
| Port      | `1883`                                   |
| Topic     | `tunisie/iot/station_meteo`              |
| Format    | `{"temperature":24.5,"humidity":60.2}`   |

> En production, remplacer par un broker privé avec authentification
> et connexion chiffrée (MQTTS / TLS).

## Comment tester

### Option 1 — Simulation Wokwi (aucun matériel requis)
1. Ouvrir le [lien de simulation](https://wokwi.com/projects/477064966556889089)
2. Lancer la simulation (▶️)
3. Ouvrir [MQTT Explorer](http://mqtt-explorer.com/), se connecter à
   `broker.hivemq.com`, s'abonner au topic `tunisie/iot/station_meteo`
4. Observer les données arriver toutes les 5 secondes

### Option 2 — Matériel réel
1. Câbler un DHT22 sur GPIO 15 (VCC → 3V3, GND → GND, DATA → GPIO15)
2. Ouvrir `main.cpp` dans Arduino IDE ou PlatformIO
3. Installer les librairies : PubSubClient, DHT sensor library, Adafruit Unified Sensor
4. Flasher sur l'ESP32, ouvrir le moniteur série (115200 bauds)

## Pistes d'évolution

- Connexion MQTT sécurisée (TLS + authentification par certificat)
- Mise à jour firmware OTA (Over-The-Air)
- Mode deep sleep entre les mesures (optimisation batterie)
- Passerelle multi-protocole (I2C/SPI vers MQTT)

---

**Ingénieur systèmes embarqués (Master)**, spécialisé ESP32 / IoT / FreeRTOS / MQTT.
Disponible pour missions freelance.
