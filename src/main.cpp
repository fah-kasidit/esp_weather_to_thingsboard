#define VERSION "1.0.0"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char *ssid = "StayTab";
const char *password = "        ";

// HTTP open_weather settings
const char *API_KEY = "017324be308ad0a6689256c147b5aeee";
const char *city_name = "Suan Luang";
const char *country_code = "TH";

// MQTT settings
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

TaskHandle_t fetch;
TaskHandle_t push;

char url[256];
String payload_input;
String payload_output;

WiFiClient espClient;
PubSubClient client(espClient);

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
      Serial.println(" try again in 1 seconds");
      delay(1000);
    }
  }
}

void build_request_url()
{
  snprintf(url, sizeof(url), "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&APPID=%s", city_name, country_code, API_KEY);
  // Serial.println(url);
}

void fetch_data(void *pvParameters)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      weather_data input_data;
      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      if (http.GET() > 0)
      {
        payload_input = http.getString();
        // Serial.println("Response payload: " + payload);

        // Get all data
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload_input);

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

        // Serial.println("City: " + input_data.City);
        // Serial.println("Country: " + input_data.Country);
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

        xQueueSend(queues, &input_data, portMAX_DELAY);
      }
      else
      {
        // url error
        Serial.println("Error on HTTP request");
      }
      http.end();
    }
    else
    {
      Serial.println("WiFi Disconnected");
      connect_wifi();
    }
    vTaskDelay(1000 * 60 / portTICK_PERIOD_MS); // 1000 milisec * 60
  }
}

void push_data(void *pvParameters)
{
  weather_data output_data;
  for (;;)
  {
    if (client.connected())
    {
      if (xQueueReceive(queues, &output_data, portMAX_DELAY))
      {
        payload_output = "{\"city\": \"" + output_data.city + "\", \"country\": \"" + output_data.country + "\", \"Weather\": \"" + output_data.weather +
                         "\", \"temp\": \"" + output_data.temp + "\", \"temp_like\": \"" + output_data.temp_like + "\", \"pressure\": \"" + output_data.pressure +
                         "\"}";
        client.publish("v1/devices/me/telemetry", payload_output.c_str(), 2);

        payload_output = "{\"humidity\": \"" + output_data.humidity + "\", \"visibility\": \"" + output_data.visibility + "\", \"wind_speed\": \"" + output_data.wind_speed +
                         "\", \"wind_deg\": \"" + output_data.wind_deg + "\", \"rain\": \"" + output_data.rain + "\", \"clouds\": \"" + output_data.clouds +
                         "\"}";
        client.publish("v1/devices/me/telemetry", payload_output.c_str(), 2);
      }
    }
    else
    {
      Serial.println("MQTT Disconnected");
      connect_mqtt();
    }
  }
}

void setup()
{
  Serial.begin(115200);
  queues = xQueueCreate(10, sizeof(weather_data));

  connect_wifi();
  connect_mqtt();

  build_request_url();

  xTaskCreatePinnedToCore(fetch_data, "Fetch Data", 20000, NULL, 2, &fetch, 0);
  xTaskCreatePinnedToCore(push_data, "push Data", 20000, NULL, 1, &push, 1);
}

void loop()
{
}