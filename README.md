# esp_weather_to_thingsboard

This mini project is about pulling weather data and pushing it into Thingsboard.

## Environment

This project requires a specific environment:

- **Board:** EPS32-S
- **Platform:** PlatformIO
- **Programming Language:** C++
- **Operating System:** Windows 11

## Configure Connection

Go to this path `".\\esp_weather_to_thingsboard\\src\\main.cpp"`

Replace all values in `{}`.

### Configure WiFi

To pull or push data, you need to connect to WiFi first.

```cpp
const char *ssid = "{Your_wifi_name}";
const char *password = "{Your_wifi_password}";
```

### Configure openWeatherMap API key

Before we config, we need to get API KEY from `openWeatherMap`.

1. Go to openWeatherMap.
2. click on `your profile`->`My API key`, you will see your API key below title name `key`.

```cpp
const char *API_KEY = "{Your_API_key}";
const char *city_name = "{Your_city_name}";
const char *country_code = "{Your_country_code}";
```

### Configure thingsboard

Before we config code, we need to create device on `Thingsboard`.

1. Go to Thingsboard.
2. Click `Entities`->`Device`.
3. Create new device.
4. Click on `Copy access token`.

Then we will config the code.

```cpp
const char *mqtt_server = "{Your_thingsboard_server}";
const char *mqtt_token = "{Your_access_token}";
```

## Upload and run the code

1. Go to this path `".\\esp_weather_to_thingsboard\\src\\main.cpp"`
2. Connect ESP32 and your computer with Upload cable.
3. Just click upload.

After upload code to ESP32 if you want to check result, go to your device in thingsboard and then click `latest telemetry`.
