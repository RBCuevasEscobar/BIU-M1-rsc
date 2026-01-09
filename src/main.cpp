/**
   ESP32 + DHT22 Example for Wokwi
   
   https://wokwi.com/arduino/projects/322410731508073042
*/

#include "DHTesp.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include "time.h"

// --- CONFIGURACIÓN DE RED Y DISPOSITIVO - ¡MODIFICA ESTOS VALORES! ---

// 1. Configuración WiFi
// const char* ssid = "INFINITUM9144_2.4";
// const char* password = "Kgd59Qs5Ep";

const char* ssid = "Wokwi-GUEST";
const char* password = "";


// 3. Endpoint de tu API en Azure
String apiEndpoint = "https://weather-app-portal-e9g0a0dggxbxggfx.westus2-01.azurewebsites.net/api/weather/readings";

// 4. Metadatos del sensor
String cityLocation = "Ciudad de Mexico"; // Ciudad por defecto
String sensorId = "sensor-222";      // ID único del sensor

// 5. Configuración del sensor y simulación

const int DHT_PIN = 15;

// Rango de presión atmosférica para la Ciudad de México (a ~2240m de altitud)
// La presión normal es ~780 hPa. Usaremos un rango realista para la simulación.
#define PRESSURE_MIN_MEXICO_CITY 77500 // 775.00 hPa
#define PRESSURE_MAX_MEXICO_CITY 78500 // 785.00 hPa

// 6. Configuración de Hora (NTP) y envío
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600; // UTC-6 para Ciudad de México (CST)
const int   daylightOffset_sec = 3600; // Horario de verano
#define     SEND_INTERVAL_MS 60000 // 60 segundos
// -------------------------------------------------------------------------

DHTesp dhtSensor;
WebServer server(80);
unsigned long lastSendTime = 0;

// Variables globales para las lecturas
float temperature, humidity, pressure;

// Declaración de funciones
void setupWiFi();
void syncTime();
void sendDataToApi();
String getFormattedTime();
void setupWebServer();
void handleRoot();
void handleUpdate();
void readSensorData();

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Iniciando Estacion Meteorologica ESP32 (Ciudad de Mexico)...");
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  Serial.println("Sensor DHT22 inicializado.");
  
  setupWiFi();
  syncTime();
  setupWebServer();
  server.begin();
  Serial.println("Servidor web iniciado. Accede a http://" + WiFi.localIP().toString());

  // Primera lectura inmediata y envio de datos.
  readSensorData(); 

  // Configura el temporizador para las siguientes lecturas periódicas.
  lastSendTime = millis();
}

void loop() {
  // Maneja las solicitudes del servidor web
  server.handleClient();

  if (millis() - lastSendTime > SEND_INTERVAL_MS) {

    // Llama a la función para la siguiente lectura de los datos del sensor y envio subsecuente.
    readSensorData();

 // Restablece el temporizador para la próxima lectura
    lastSendTime = millis();
  }
}

void readSensorData() {
  Serial.println("---");
  Serial.println("Leyendo datos del sensor para envio...");
  TempAndHumidity  data = dhtSensor.getTempAndHumidity();
  temperature = data.temperature;
  humidity = data.humidity;
  // -- Generación de Presión Atmosférica Aleatoria --
  // Genera un número entero en el rango y luego lo convierte a flotante
  pressure = random(PRESSURE_MIN_MEXICO_CITY, PRESSURE_MAX_MEXICO_CITY) / 100.0;
  // Los sensores DHT a veces fallan. Es vital comprobar si la lectura es válida.
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Error: Fallo al leer del sensor DHT22. Revisa las conexiones.");
    return;
  }
  Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("Pressure: " + String(pressure, 1) + "hPa");
  Serial.println("---");
  sendDataToApi();
}

// Implementación de funciones auxiliares

void setupWiFi() {
  Serial.println("Configurando WiFi...");

  WiFi.begin(ssid, password, 6);
  Serial.print("Conectando a: "); Serial.println(ssid);
  
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++retries > 20) {
      Serial.println("\nNo se pudo conectar. Revisa tus credenciales o la configuración de red.");
      return;
    }
  }

  Serial.println("\n¡WiFi conectado!");
  Serial.print("Dirección IP asignada: "); Serial.println(WiFi.localIP());
}

void sendDataToApi() {
  if (WiFi.status() != WL_CONNECTED || isnan(temperature)) {
    Serial.println("No se enviarán datos: Sin WiFi o existe lectura de sensor inválida.");
    return;
  }

  JsonDocument doc;
  
  doc["timestamp"] = getFormattedTime();  
  doc["location"] = cityLocation;
  doc["sensorId"] = sensorId;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["pressure"] = pressure;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  HTTPClient http;
  http.begin(apiEndpoint);
  http.addHeader("Content-Type", "application/json");

  Serial.println("Enviando JSON a la API: " + jsonPayload);
  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.print("Código de respuesta HTTP: "); Serial.println(httpResponseCode);
  } else {
    Serial.print("Error en la petición POST. Código: "); Serial.println(httpResponseCode);
  }
  http.end();
}

void syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("Sincronizando hora...");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(" Fallo al obtener la hora.");
    return;
  }
  Serial.println(" Hora sincronizada.");
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "TIME_ERROR";
  }
  char timeStringBuff[20]; // Formato ISO 8601: YYYY-MM-DDTHH:MM:SS
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void handleRoot() {
  String html = "<html><head><title>Estacion Meteorologica ESP32</title>";
  html += "<style>body{font-family: Arial, sans-serif;} h1{color: #005691;} .data{font-size:1.2em;}</style></head>";
  html += "<body><h1>Estado de la Estacion Meteorologica</h1>";
  html += "<h2>ID del Sensor: " + sensorId + "</h2>";
  html += "<h2>Ubicacion: " + cityLocation + "</h2>";
  html += "<hr>";
  html += "<h2>Lecturas Actuales</h2>";
  html += "<p class='data'><b>Temperatura:</b> " + String(temperature) + " &deg;C</p>";
  html += "<p class='data'><b>Humedad:</b> " + String(humidity) + " %</p>";
  html += "<p class='data'><b>Presion:</b> " + String(pressure) + " hPa</p>";
  html += "<hr>";
  html += "<h2>Configurar Dispositivo</h2>";
  html += "<form action='/update' method='post'>";
  html += "ID del Sensor: <input type='text' name='sensorId' value='" + sensorId + "'><br><br>";
  html += "Ubicacion (Ciudad): <input type='text' name='location' value='" + cityLocation + "'><br><br>";
  html += "<input type='submit' value='Actualizar'>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleUpdate() {
  if (server.hasArg("sensorId") && server.hasArg("location")) {
    sensorId = server.arg("sensorId");
    cityLocation = server.arg("location");
    Serial.println("Configuracion actualizada via web:");
    Serial.println("Nuevo ID: " + sensorId);
    Serial.println("Nueva Ubicacion: " + cityLocation);
    server.send(200, "text/plain", "Configuracion actualizada. Redirigiendo...");
    // Redirige al usuario a la página principal después de 2 segundos
    server.sendHeader("Refresh", "2; url=/");
  } else {
    server.send(400, "text/plain", "Error: Faltan argumentos.");
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);
}