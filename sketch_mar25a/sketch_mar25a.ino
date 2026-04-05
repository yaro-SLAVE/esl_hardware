#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <QRCodeGFX.h>

#include <FastLED.h>

// Константы для пинов
#define CS_PIN (15)
#define BUSY_PIN (16)
#define RES_PIN (0)
#define DC_PIN (2)

#define LED_PIN 3  
#define NUM_LEDS 3
#define BUTTON_PIN (12)

// Структура для хранения конфигурации
struct Config {
  char ssid[32];
  char password[64];
  char serverDomain[64];
  int rackNumber;
  bool configured;
};

Config config;
ESP8266WebServer server(80);
bool configMode = false; // По умолчанию не в режиме конфигурации
bool buttonPressed = false;
unsigned long buttonPressStartTime = 0;

CRGB leds[NUM_LEDS];

// GxEPD2 дисплей
GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(GxEPD2_213_Z98c(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN));
U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;
QRCodeGFX qrcode(display);

const String validToken = "qwe123123qwe";

bool promotionActive = false;

// HTML страница для конфигурации
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Настройка ESP8266</title>
    <style>
        input, 
        input[type="text"], 
        input[type="password"], 
        input[type="number"],
        textarea,
        select {
            -webkit-touch-callout: default !important;
            -webkit-user-select: text !important;
            user-select: text !important;
        }
        body {
            font-family: Arial, sans-serif;
            max-width: 500px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
            margin-bottom: 30px;
        }
        h2 {
            color: #666;
            font-size: 18px;
            margin-bottom: 20px;
            text-align: center;
        }
        label {
            display: block;
            margin: 15px 0 5px;
            font-weight: bold;
            color: #555;
        }
        select, input[type="password"], input[type="text"], input[type="number"] {
            width: 100%;
            padding: 12px;
            margin-bottom: 20px;
            border: 1px solid #ddd;
            border-radius: 5px;
            box-sizing: border-box;
            font-size: 16px;
        }
        button {
            width: 100%;
            padding: 15px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 18px;
            font-weight: bold;
            margin-top: 10px;
            transition: background-color 0.3s;
        }
        button:hover {
            background-color: #45a049;
        }
        .status {
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            text-align: center;
            display: none;
        }
        .success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .network-info {
            background-color: #f8f9fa;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
            text-align: center;
            border-left: 4px solid #4CAF50;
        }
        .refresh-btn {
            background-color: #2196F3;
            margin-bottom: 20px;
            padding: 10px;
            font-size: 14px;
        }
        .loader {
            border: 4px solid #f3f3f3;
            border-top: 4px solid #3498db;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            margin: 20px auto;
            display: none;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Настройка ESP8266</h1>
        <div class="network-info">
            <strong>Точка доступа:</strong> ESP8266-Config<br>
            <strong>IP адрес:</strong> 192.168.4.1
        </div>
        
        <div id="status" class="status"></div>
        
        <h2>Настройка Wi-Fi подключения</h2>
        
        <button class="refresh-btn" onclick="loadNetworks()">Обновить список сетей</button>
        
        <form id="configForm">
            <label for="ssid">Выберите Wi-Fi сеть:</label>
            <select id="ssid" name="ssid" required>
                <option value="">Загрузка сетей...</option>
            </select>
            
            <label for="password">Пароль Wi-Fi:</label>
            <input type="password" id="password" name="password" placeholder="Введите пароль от Wi-Fi">

            <label for="rackNumber">Номер стеллажа</label>
            <input type="number" id="rackNumber" name="rackNumber" placeholder="Введите номер стеллажа">
            
            <label for="serverDomain">Домен главного сервера:</label>
            <input type="text" id="serverDomain" name="serverDomain" 
                   placeholder="example.com или IP адрес" 
                   value="%SERVERDOMAIN%" required>
            
            <button type="submit">Сохранить настройки</button>
        </form>
        
        <div id="loader" class="loader"></div>
        
        <div style="margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee;">
            <p style="text-align: center; color: #777; font-size: 14px;">
                После сохранения устройство перезагрузится и попытается подключиться к указанной сети Wi-Fi.
            </p>
        </div>
    </div>
    
    <script>
        window.onload = function() {
            loadNetworks();
            document.getElementById('serverDomain').value = '%SERVERDOMAIN%';
        };
        
        function loadNetworks() {
            const select = document.getElementById('ssid');
            select.innerHTML = '<option value="">Сканирование сетей...</option>';
            select.disabled = true;
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    select.innerHTML = '<option value="">Выберите сеть...</option>';
                    if (data.networks && data.networks.length > 0) {
                        data.networks.sort((a, b) => b.rssi - a.rssi);
                        data.networks.forEach(network => {
                            const option = document.createElement('option');
                            option.value = network.ssid;
                            let signalStrength = '';
                            if (network.rssi >= -50) signalStrength = 'Отличный';
                            else if (network.rssi >= -60) signalStrength = 'Хороший';
                            else if (network.rssi >= -70) signalStrength = 'Средний';
                            else signalStrength = 'Слабый';
                            option.textContent = `${network.ssid} (${signalStrength}, ${network.rssi} dBm)`;
                            select.appendChild(option);
                        });
                    } else {
                        select.innerHTML = '<option value="">Сети не найдены</option>';
                    }
                    select.disabled = false;
                })
                .catch(error => {
                    select.innerHTML = '<option value="">Ошибка загрузки</option>';
                    select.disabled = false;
                });
        }
        
        function showStatus(message, type) {
            const statusDiv = document.getElementById('status');
            statusDiv.textContent = message;
            statusDiv.className = 'status ' + type;
            statusDiv.style.display = 'block';
            setTimeout(() => { statusDiv.style.display = 'none'; }, 5000);
        }
        
        document.getElementById('configForm').onsubmit = function(e) {
            e.preventDefault();
            
            const loader = document.getElementById('loader');
            const submitButton = document.querySelector('button[type="submit"]');
            const ssidSelect = document.getElementById('ssid');
            
            if (!ssidSelect.value) {
                showStatus('Пожалуйста, выберите Wi-Fi сеть', 'error');
                return false;
            }
            
            loader.style.display = 'block';
            submitButton.disabled = true;
            submitButton.textContent = 'Сохранение...';
            
            const formData = {
                ssid: ssidSelect.value,
                password: document.getElementById('password').value,
                serverDomain: document.getElementById('serverDomain').value,
                rackNumber: document.getElementById('rackNumber').value
            };
            
            fetch('/save', {
                method: 'POST',
                body: JSON.stringify(formData),
                headers: { 'Content-Type': 'application/json' }
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showStatus('Настройки сохранены! Устройство перезагрузится...', 'success');
                    setTimeout(() => { window.location.href = '/'; }, 3000);
                } else {
                    throw new Error(data.message);
                }
            })
            .catch(error => {
                showStatus('Ошибка сохранения: ' + error.message, 'error');
                submitButton.textContent = 'Сохранить настройки';
            })
            .finally(() => {
                loader.style.display = 'none';
                submitButton.disabled = false;
            });
        };
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("       ESP8266 Универсальный v1.0       ");
  Serial.println("========================================");
  
  // Инициализация пинов
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println(digitalRead(12));
  Serial.println(digitalRead(13));
  Serial.println(digitalRead(14));
  Serial.println(digitalRead(16));
  Serial.println(digitalRead(15));
  
  // Инициализация LED ленты
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  FastLED.clear();
  FastLED.show();
  
  // Инициализация дисплея
  display.init();
  u8g2_for_adafruit_gfx.begin(display);
  
  // Загрузка конфигурации из EEPROM
  EEPROM.begin(sizeof(Config));
  delay(100);
  loadConfig();
  
  // Проверка кнопки при старте (если зажата - сразу в режим конфигурации)
  if (digitalRead(BUTTON_PIN) == HIGH) {
    Serial.println("Кнопка зажата при старте - вход в режим конфигурации");
    startConfigMode();
  } else if (config.configured && strlen(config.ssid) > 0) {    
    // Попытка подключения к сохраненной Wi-Fi сети
    attemptWiFiConnection();
    if (WiFi.status() == WL_CONNECTED) {
      configMode = false;
      setupNormalModeWebServer();
    } else {
      startConfigMode();
    }
  } else {
    startConfigMode();
  }
}

void loop() {
  server.handleClient();
  handleResetButton();
  
  // Проверка соединения в обычном режиме
  // if (!configMode && config.configured && WiFi.status() != WL_CONNECTED) {
  //   static unsigned long lastReconnectAttempt = 0;
  //   if (millis() - lastReconnectAttempt > 30000) {
  //     Serial.println("Потеряно соединение с Wi-Fi. Попытка переподключения...");
  //     WiFi.reconnect();
  //     lastReconnectAttempt = millis();
      
  //     if (WiFi.status() != WL_CONNECTED) {
  //       Serial.println("Не удалось переподключиться. Переход в режим конфигурации...");
  //       startConfigMode();
  //     }
  //   }
  // }
}

void startConfigMode() {
  configMode = true;
  Serial.println("Вход в режим конфигурации");
  
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  
  String apName = "ESP8266-Config";
  WiFi.softAP(apName.c_str());
  
  Serial.println("\n════════════════════════════════════════");
  Serial.println("Точка доступа запущена:");
  Serial.print("  SSID: "); Serial.println(apName);
  Serial.print("  IP адрес: "); Serial.println(WiFi.softAPIP());
  Serial.println("════════════════════════════════════════\n");
  
  setupConfigWebServer();
  
  // Мигание светодиодами в режиме конфигурации
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Blue;
  }
  FastLED.show();
}

void setupConfigWebServer() {
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/save", handleSave);
  server.on("/reset", handleReset);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Веб-сервер конфигурации запущен");
  Serial.println("Откройте http://" + WiFi.softAPIP().toString());

  display.fillScreen(GxEPD_BLACK);
  display.setRotation(1);
  
  qrcode.setScale(2);
  qrcode.draw("http://" + WiFi.softAPIP().toString(), 10, 50);
  
  display.display(true);
}

void setupNormalModeWebServer() {
  configMode = false;
  Serial.println("Вход в обычный режим работы");
  
  server.on("/api/product/", HTTP_POST, handleProductPost);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP сервер запущен в обычном режиме");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());
  
  // Зеленый светодиод - нормальный режим
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Green;
  }
  FastLED.show();
}

void attemptWiFiConnection() {
  Serial.println("\nПопытка подключения к сохраненной Wi-Fi сети...");
  Serial.print("SSID: ");
  Serial.println(config.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Мигание синим во время подключения
    static bool ledState = false;
    ledState = !ledState;
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = ledState ? CRGB::Blue : CRGB::Black;
    }
    FastLED.show();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nУСПЕШНО ПОДКЛЮЧЕНО!");
    Serial.print("  IP адрес: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Сигнал: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\nНЕ УДАЛОСЬ ПОДКЛЮЧИТЬСЯ");
  }
}

void handleResetButton() {
  static bool lastButtonState = LOW;
  static unsigned long pressStartTime = 0;

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  bool currentButtonState = !digitalRead(BUTTON_PIN);
  
  // if (currentButtonState == HIGH && lastButtonState == LOW) {
  //   pressStartTime = millis();
  //   Serial.println("Кнопка нажата");
    
  //   // Визуальная индикация нажатия
  //   for (int i = 0; i < NUM_LEDS; i++) {
  //     leds[i] = CRGB::White;
  //   }
  //   FastLED.show();
  // }
  
  if (currentButtonState == HIGH) {
    unsigned long pressDuration = millis() - pressStartTime;
    
    if (pressDuration >= 3000 && !configMode) {
      Serial.println("Кнопка удержана 3 секунды. Переход в режим конфигурации...");
      
      // Мигание красным перед переходом
      // for (int blink = 0; blink < 5; blink++) {
      //   for (int i = 0; i < NUM_LEDS; i++) {
      //     leds[i] = CRGB::Red;
      //   }
      //   FastLED.show();
      //   delay(200);
      //   for (int i = 0; i < NUM_LEDS; i++) {
      //     leds[i] = CRGB::Black;
      //   }
      //   FastLED.show();
      //   delay(200);
      // }
      
      startConfigMode();
      pressStartTime = millis(); // Сброс таймера
    }
  }
  
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    unsigned long pressDuration = millis() - pressStartTime;
    
    if (pressDuration >= 5000 && configMode) {
      Serial.println("Кнопка удержана 5 секунд в режиме конфигурации. Сброс настроек...");
      resetConfig();
      
      for (int blink = 0; blink < 10; blink++) {
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Red;
        }
        FastLED.show();
        delay(100);
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
        }
        FastLED.show();
        delay(100);
      }
      
      ESP.restart();
    }
    
    // Восстановление нормальной индикации
    if (!configMode && WiFi.status() == WL_CONNECTED) {
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Green;
      }
    } else if (configMode) {
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Blue;
      }
    }
    FastLED.show();
  }
  
  lastButtonState = currentButtonState;
}

void handleProductPost() {
  Serial.println("Received POST request to /api/product/");
  
  if (server.method() != HTTP_POST) {
    sendError(405, "Method Not Allowed");
    return;
  }
  
  if (!server.hasHeader("Authorization")) {
    sendError(401, "Missing Authorization header");
    return;
  }
  
  String authHeader = server.header("Authorization");
  Serial.println("Authorization header: " + authHeader);
  
  if (!isValidBearerToken(authHeader)) {
    sendError(401, "Invalid or expired token");
    return;
  }
  
  if (!server.hasArg("plain")) {
    sendError(400, "Missing request body");
    return;
  }
  
  String requestBody = server.arg("plain");
  Serial.println("Request body: " + requestBody);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, requestBody);
  
  if (error) {
    sendError(400, "Invalid JSON format: " + String(error.c_str()));
    return;
  }
  
  String productName = doc["short_name"].as<String>();
  float price = doc["price"].as<float>();
  String product = doc["product"].as<String>();
  String company = doc["company"].as<String>();
  int promotion = doc["promotion"].as<int>() | 0;
  bool havePromotion = doc["have_promotion"].as<bool>() | false;
  
  Serial.println("Product received:");
  Serial.println("  Name: " + productName);
  Serial.println("  Price: " + String(price));
  Serial.println("  ID: " + String(product));
  
  DynamicJsonDocument responseDoc(256);
  responseDoc["status"] = "success";
  responseDoc["message"] = "Product received successfully";
  responseDoc["product_name"] = productName;
  responseDoc["received_at"] = millis();
  
  String response;
  serializeJson(responseDoc, response);
  
  server.send(200, "application/json", response);
  
  if (havePromotion) {
    activatePromotionLight();
  }
  
  // Отображение на дисплее
  display.fillScreen(GxEPD_BLACK);
  display.setRotation(1);
  
  u8g2_for_adafruit_gfx.setFontMode(1);
  u8g2_for_adafruit_gfx.setFontDirection(0);
  u8g2_for_adafruit_gfx.setFont(u8g2_font_10x20_t_cyrillic);
  u8g2_for_adafruit_gfx.setCursor(10, 20);
  u8g2_for_adafruit_gfx.print(productName);
  u8g2_for_adafruit_gfx.setCursor(10, 40);
  u8g2_for_adafruit_gfx.print(String(price));
  
  qrcode.setScale(2);
  qrcode.draw("http://" + String(config.serverDomain) + "/#/product/?product=" + product + "&company=" + company, 10, 50);
  
  display.display(true);
}

void activatePromotionLight() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
  delay(3000);
  
  // Возврат к нормальной индикации
  if (!configMode && WiFi.status() == WL_CONNECTED) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Green;
    }
  } else if (configMode) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Blue;
    }
  }
  FastLED.show();
}

bool isValidBearerToken(const String& authHeader) {
  String bearerPrefix = "Bearer ";
  
  if (!authHeader.startsWith(bearerPrefix)) {
    return false;
  }
  
  String token = authHeader.substring(bearerPrefix.length());
  token.trim();
  
  Serial.println("Extracted token: " + token);
  Serial.println("Expected token: " + validToken);
  
  return token == validToken;
}

void sendError(int code, const String& message) {
  DynamicJsonDocument doc(256);
  doc["status"] = "error";
  doc["code"] = code;
  doc["message"] = message;
  
  String response;
  serializeJson(doc, response);
  
  server.send(code, "application/json", response);
  
  Serial.println("Error " + String(code) + ": " + message);
}

// Функции для режима конфигурации
void handleRoot() {
  String page = htmlPage;
  page.replace("%SERVERDOMAIN%", config.serverDomain);
  server.send(200, "text/html", page);
}

void handleScan() {
  Serial.println("Сканирование Wi-Fi сетей...");
  
  int n = WiFi.scanNetworks(false, true);
  
  String json = "{\"networks\":[";
  
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + escapeJson(WiFi.SSID(i)) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  
  json += "]}";
  
  server.send(200, "application/json", json);
  Serial.print("Найдено сетей: ");
  Serial.println(n);
}

void handleSave() {
  if (server.hasArg("plain")) {
    String json = server.arg("plain");
    Serial.println("Получены данные для сохранения:");
    Serial.println(json);
    
    String ssid = extractJsonValue(json, "ssid");
    String password = extractJsonValue(json, "password");
    String serverDomain = extractJsonValue(json, "serverDomain");
    String rackNumberStr = extractJsonValue(json, "rackNumber");
    int rackNumber = rackNumberStr.toInt();
    
    if (ssid == "") ssid = extractJsonValueUnquoted(json, "ssid");
    if (password == "") password = extractJsonValueUnquoted(json, "password");
    if (serverDomain == "") serverDomain = extractJsonValueUnquoted(json, "serverDomain");
    if (rackNumberStr == "") rackNumberStr = extractJsonValueUnquoted(json, "rackNumber");
    
    ssid.replace("\"", "");
    password.replace("\"", "");
    serverDomain.replace("\"", "");
    
    if (ssid == "" || serverDomain == "") {
      String response = "{\"success\":false,\"message\":\"Неверные данные: SSID и домен сервера обязательны\"}";
      server.send(400, "application/json", response);
      return;
    }
    
    ssid.toCharArray(config.ssid, sizeof(config.ssid));
    password.toCharArray(config.password, sizeof(config.password));
    serverDomain.toCharArray(config.serverDomain, sizeof(config.serverDomain));
    config.rackNumber = rackNumber;
    config.configured = true;
    
    saveConfig();
    
    Serial.println("Конфигурация сохранена:");
    Serial.print("  SSID: ");
    Serial.println(config.ssid);
    Serial.print("  Сервер: ");
    Serial.println(config.serverDomain);
    Serial.print("  Номер стеллажа: ");
    Serial.println(config.rackNumber);
    
    String response = "{\"success\":true,\"message\":\"Конфигурация сохранена\"}";
    server.send(200, "application/json", response);
    
    delay(1000);

    attemptWiFiConnection();
    if (WiFi.status() == WL_CONNECTED) {
      configMode = false;

      WiFiClient client;
      
      if (client.connect(config.serverDomain, 8000)) {
        // Подготовка JSON данных
        String jsonData = "{\"token\":\"" + validToken + "\",\"rack\":\"" + config.rackNumber + "\"}";
        
        // Отправка POST запроса
        client.println("POST /api/esl/ HTTP/1.1");
        client.println("Host: " + String(config.serverDomain));
        client.println("Content-Type: application/json");
        client.print("Content-Length: ");
        client.println(jsonData.length());
        client.println();
        client.println(jsonData);
        
        // Чтение ответа
        while (client.connected() || client.available()) {
          if (client.available()) {
            String line = client.readStringUntil('\n');
            Serial.println(line);
          }
        }
        client.stop();
      } else {
        Serial.println("Connection failed");
      }

      setupNormalModeWebServer();
    }

    // Serial.println("Перезагрузка...");
    // ESP.restart();
    
  } else {
    String response = "{\"success\":false,\"message\":\"Нет данных\"}";
    server.send(400, "application/json", response);
  }
}

void handleReset() {
  resetConfig();
  String response = "{\"success\":true,\"message\":\"Настройки сброшены\"}";
  server.send(200, "application/json", response);
  delay(1000);
  ESP.restart();
}

void handleStatus() {
  String json = "{";
  json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"connected\":";
  json += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  if (WiFi.status() == WL_CONNECTED) {
    json += ",\"sta_ip\":\"" + WiFi.localIP().toString() + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI());
  }
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleNotFound() {
  String message = "Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}

String extractJsonValue(String json, String key) {
  String searchKey = "\"" + key + "\":";
  int keyIndex = json.indexOf(searchKey);
  if (keyIndex == -1) return "";
  
  int valueStart = json.indexOf("\"", keyIndex + searchKey.length());
  if (valueStart == -1) return "";
  
  int valueEnd = json.indexOf("\"", valueStart + 1);
  if (valueEnd == -1) return "";
  
  return json.substring(valueStart + 1, valueEnd);
}

String extractJsonValueUnquoted(String json, String key) {
  String searchKey = "\"" + key + "\":";
  int keyIndex = json.indexOf(searchKey);
  if (keyIndex == -1) return "";
  
  int valueStart = keyIndex + searchKey.length();
  
  while (valueStart < json.length() && json.charAt(valueStart) == ' ') {
    valueStart++;
  }
  
  int valueEnd = valueStart;
  
  while (valueEnd < json.length() && 
         json.charAt(valueEnd) != ',' && 
         json.charAt(valueEnd) != '}' &&
         json.charAt(valueEnd) != ' ' &&
         json.charAt(valueEnd) != '\n' &&
         json.charAt(valueEnd) != '\r' &&
         json.charAt(valueEnd) != '\t') {
    valueEnd++;
  }
  
  return json.substring(valueStart, valueEnd);
}

void loadConfig() {
  EEPROM.get(0, config);
  
  if (config.configured != true || 
      strlen(config.ssid) == 0 || 
      strlen(config.serverDomain) == 0) {
    resetConfig();
  }
  
  Serial.println("Загруженная конфигурация:");
  Serial.print("  Настроено: ");
  Serial.println(config.configured ? "Да" : "Нет");
  Serial.print("  SSID: ");
  Serial.println(config.ssid);
  Serial.print("  Сервер: ");
  Serial.println(config.serverDomain);
  Serial.print("  Номер стеллажа: ");
  Serial.println(config.rackNumber);
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
  Serial.println("Конфигурация сохранена в EEPROM");
}

void resetConfig() {
  memset(&config, 0, sizeof(config));
  strcpy(config.serverDomain, "example.com");
  config.configured = false;
  saveConfig();
  Serial.println("Конфигурация сброшена к значениям по умолчанию");
}

String escapeJson(String input) {
  input.replace("\\", "\\\\");
  input.replace("\"", "\\\"");
  input.replace("\n", "\\n");
  input.replace("\r", "\\r");
  input.replace("\t", "\\t");
  return input;
}