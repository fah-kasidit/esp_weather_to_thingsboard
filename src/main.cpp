#define VERSION "1.0.1"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char *ssid = "NDRS-Wongsawang";
const char *password = "ndrs_2010";

const char *API_KEY = "017324be308ad0a6689256c147b5aeee";
const char *city_name = "Suan Luang";
const char *country_code = "TH";

const char *mqtt_server = "143.198.195.172";
const char *mqtt_token = "i6hqZKbqWYxyHJVawiCy";

struct weather_data
{
  String city;
  String country;

  String weather;
  String temp;
  String temp_like;

  String pressure;
  String humidity;
  String visibility;

  String wind_speed;
  String wind_deg;

  String rain;
  String clouds;
};

QueueHandle_t queues;
SemaphoreHandle_t queueMutex;
TaskHandle_t fetch;
TaskHandle_t push;

char url[128];
char payload[1024];

WiFiClient espClient;
PubSubClient client(espClient);

/**
 * @brief Funtion for connect to wifi.
 *
 * Using this funtion when want to connect to wifi or new connect to wifi.
 */
void connect_wifi()
{
  // connect wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

/**
 * @brief Funtion for connect to MQTT server.
 *
 * Using this funtion when want to connect to mqtt server.
 */
void connect_mqtt()
{
  // connect mqtt
  client.setServer(mqtt_server, 1883);

  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_token, NULL))
    {
      Serial.println("MQTT connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 1 second");
      delay(1000);
    }
  }
}

/**
 * @brief Funtion for build url.
 *
 * Using this funtion when want to build http url for request data from openweathermap.
 */
void build_request_url()
{
  snprintf(url, sizeof(url), "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&APPID=%s", city_name, country_code, API_KEY);
  Serial.println("\n" + String(url) + "\n");
}

/**
 * @brief Funtion for fetch data from the builded url.
 *
 * Using this funtion when want to fetch new data.
 * 
 * @param pvParameters Pointer to the parameters required for fetching data.
 *                     This can be used to pass additional context or configuration
 *                     needed by the function.
 */
void fetch_data(void *pvParameters)
{
  weather_data input_data;
  DynamicJsonDocument doc(1024);
  HTTPClient http;

  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      if (http.GET() > 0)
      {
        snprintf(payload, sizeof(payload), (http.getString()).c_str());
        // Serial.println("Response payload: " + String(payload));

        deserializeJson(doc, payload);

        input_data.city = doc["name"].as<String>();
        input_data.country = doc["sys"]["country"].as<String>();
        input_data.weather = doc["weather"][0]["description"].as<String>();
        input_data.temp = String(doc["main"]["temp"].as<float>() - 273.15);
        input_data.temp_like = String(doc["main"]["feels_like"].as<float>() - 273.15);
        input_data.pressure = doc["main"]["pressure"].as<String>();
        input_data.humidity = doc["main"]["humidity"].as<String>();
        input_data.visibility = doc["visibility"].as<String>();
        input_data.wind_speed = doc["wind"]["speed"].as<String>();
        input_data.wind_deg = doc["wind"]["deg"].as<String>();
        input_data.rain = doc.containsKey("rain") ? doc["rain"]["1h"].as<String>() : "0";
        input_data.clouds = doc["clouds"]["all"].as<String>();

        // Serial.println("City: " + input_data.city);
        // Serial.println("Country: " + input_data.country);
        // Serial.println("Weather: " + input_data.weather);
        // Serial.println("Temperature: " + input_data.temp + " °C");
        // Serial.println("Feels Like: " + input_data.temp_like + " °C");
        // Serial.println("Pressure: " + input_data.pressure + " hPa");
        // Serial.println("Humidity: " + input_data.humidity + " %");
        // Serial.println("Visibility: " + input_data.visibility + " m");
        // Serial.println("Wind Speed: " + input_data.wind_speed + " m/s");
        // Serial.println("Wind Degree: " + input_data.wind_deg + " °");
        // Serial.println("Rain (1h): " + input_data.rain + " mm");
        // Serial.println("Cloudiness: " + input_data.clouds + " %\n");

        if (xSemaphoreTake(queueMutex, portMAX_DELAY) == pdTRUE)
        {
          xQueueSend(queues, &input_data, portMAX_DELAY);
          Serial.println("Fetch data");
          xSemaphoreGive(queueMutex);
        }
        vTaskDelay(1000 * 60 / portTICK_PERIOD_MS);
      }
      else
      {
        Serial.println("Error on HTTP request");
      }
      http.end();
    }
    else
    {
      Serial.println("WiFi Disconnected");
      connect_wifi();
    }
  }
}

/**
 * @brief Funtion for push data.
 *
 * Using this funtion when want to push data to mqtt server.
 * @param pvParameters Pointer to the parameters required for pushing data.
 *                     This can be used to pass additional context or configuration
 *                     needed by the function.
 */
void push_data(void *pvParameters)
{
  weather_data output_data;
  for (;;)
  {
    if (client.connected())
    {
      if (uxQueueMessagesWaiting(queues) > 0)
      {
        if (xSemaphoreTake(queueMutex, portMAX_DELAY) == pdTRUE)
        {
          if (xQueueReceive(queues, &output_data, portMAX_DELAY))
          {
            snprintf(payload, sizeof(payload), "{\"city\": \"%s\", \"country\": \"%s\", \"Weather\": \"%s\", \"temp\": \"%s\", \"temp_like\": \"%s\", \"pressure\": \"%s\"}", output_data.city, output_data.country, output_data.weather, output_data.temp, output_data.temp_like, output_data.pressure);
            client.publish("v1/devices/me/telemetry", payload, 2);

            snprintf(payload, sizeof(payload), "{\"humidity\": \"%s\", \"visibility\": \"%s\", \"wind_speed\": \"%s\", \"wind_deg\": \"%s\", \"rain\": \"%s\", \"clouds\": \"%s\"}", output_data.humidity, output_data.visibility, output_data.wind_speed, output_data.wind_deg, output_data.rain, output_data.clouds);
            client.publish("v1/devices/me/telemetry", payload, 2);
          }
          Serial.println("Push data");
          xSemaphoreGive(queueMutex);
        }
      }
    }
    else
    {
      Serial.println("MQTT Disconnected");
      connect_mqtt();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  queueMutex = xSemaphoreCreateMutex();
  queues = xQueueCreate(10, sizeof(weather_data));

  connect_wifi();
  connect_mqtt();
  build_request_url();

  xTaskCreatePinnedToCore(fetch_data, "Fetch Data", 4096, NULL, 1, &fetch, 0);
  xTaskCreatePinnedToCore(push_data, "Push Data", 4096, NULL, 1, &push, 1);
}

void loop()
{
}