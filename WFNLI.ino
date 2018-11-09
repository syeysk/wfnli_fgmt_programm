/*
 * @author: Polyakov Konstantin
 * 
 */

#define LANG_RU 1
//#define LANG_EN 1

#define DEVICE_TYPE "wfnli"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ArduinoJson.h>

#define PIN_R 16
#define PIN_G 12
#define PIN_B 13
#define PIN_Brightness 5
#define PIN_BTN_LEFT 4
#define PIN_BTN_RIGHT 5

ESP8266WebServer webServer(80);

byte pin_btn_reset = 14;

// '#' + parseInt(16642312).toString(16).padStart(6, '0');
unsigned int colors[] = {
                         16729344, // orange
                       //16642312, // yellow #fdf108
                         16645376, // yellow #fdfd00
                       //7927813, // green #78f805
                         64768, // green #00fd00
                         586492, // light blue #08f2fc
                       //1333986, // blue #145ae2
                         1052925, // blue #1010fd
                       //14418175, // пурпурный #dc00ff
                         16523503, // пурпурный #fc20ef
                       //16268306, // red #f83c12
                         15597568, // red #ee0000
                         };
int current_color_index = 0;

void select_color(int direction) {
    current_color_index += direction;
    if (current_color_index < 0) current_color_index = 6;
    if (current_color_index > 6) current_color_index = 0;

    unsigned int color = colors[current_color_index];

    set_color(color);
    save_color(color);
}

volatile unsigned int is_btn_pressed = 0;
byte max_press = 1;

void select_prev_color() {
    //delay(50);
    //if (digitalRead(PIN_BTN_LEFT)==1) return;

    //if (is_btn_pressed < max_press) {is_btn_pressed += 1; return;}

    if (is_btn_pressed == 0) {is_btn_pressed = 1;}
    else {return;}
    
    select_color(-1);

    //delay(100);

    is_btn_pressed = 0;
}

void select_next_color() {

    unsigned int x = 0;
    while(x < 32000) x+=1;
    x = 0;
    while(x < 32000) x+=1;
    x = 0;
    while(x < 32000) x+=1;
    x = 0;
    while(x < 32000) x+=1;
    x = 0;
  
    //if (digitalRead(PIN_BTN_RIGHT)==1) return;

    //if (is_btn_pressed < max_press) {is_btn_pressed += 1; return;}

    //if (is_btn_pressed == 0) {is_btn_pressed = 1;}
    //else {return;}
 
    select_color(1);
    //delay(100);

    //is_btn_pressed = 0;
}

// состояния каналов реле мы не храним в структуре, а отдельно,
// чтобы при каждом изменении сотсояния не трогать настройки.
struct DefaultSettings {
    byte wifi_mode = 0; // 0 - точка доступа, 1 - клиент, 2 - оба варианта одновременно

    char ssidAP[32] = "WiFi_NightLight";
    char passwordAP[32] = "";

    char ssid[32] = "Your name";
    char password[32] = "Your password";

    char device_name[64] = "Device name";

    unsigned int update_time = 2000;
};

struct WFRStatistic {
    float vcc = 0;
    String time_s = "00";
    String time_m = "00";
    String time_h = "00";
};

// eeprom addresses
const unsigned int ee_addr_start_firstrun = 0;
const unsigned int ee_addr_start_color = 1; // Red Green Blue Brightness
const unsigned int ee_addr_start_demo = 5;
const unsigned int ee_addr_start_demo_speed = 6;
const unsigned int ee_addr_start_settings = 7;

const byte code_firstrun = 4;

DefaultSettings ee_data;
WFRStatistic stat;

byte is_wifi_client_connected = 0;
byte is_turn_on = 1;
byte is_demo = 0;
Ticker ticker_demo;
byte demo_current_color_index = 0;
byte demo_stps[] = {0, 0};

void notFoundHandler() {
    webServer.send(404, "text/html", "<h1>Not found :-(</h1>");
}

void set_color(unsigned int color) {
    byte r = (color >> 16) & 0xff;
    byte g = (color >> 8) & 0xff;
    byte b = color & 0xff;
    Serial.println("Color = "+String(r, DEC)+", "+String(g, DEC)+", "+String(b, DEC));
    analogWrite(PIN_R, r);
    analogWrite(PIN_G, g);
    analogWrite(PIN_B, b);
}
void save_color(unsigned int color) {
    unsigned int full_color;
    EEPROM.get(ee_addr_start_color, full_color);
    full_color = (full_color & 0xff) + (color << 8);
    EEPROM.put(ee_addr_start_color, full_color);
    EEPROM.commit();
}
unsigned int read_color() {
    unsigned int full_color;
    EEPROM.get(ee_addr_start_color, full_color);
    return full_color >> 8;
}

void set_brightness(byte brightness) {
    brightness = brightness & 0xff;
    analogWrite(PIN_Brightness, brightness);
}
void save_brightness(byte brightness) {
    unsigned int full_color;
    brightness = brightness & 0xff;
    EEPROM.get(ee_addr_start_color, full_color);
    full_color = (full_color & 0xffffff00) + brightness;
    EEPROM.put(ee_addr_start_color, full_color);
    EEPROM.commit();
}
byte read_brightness() {
    unsigned int full_color;
    EEPROM.get(ee_addr_start_color, full_color);
    return full_color & 0xff;
}

void turn(byte t) {
    is_turn_on = t;
    if (t == 0) {
        set_color(0);//set_brightness(t);
    } else {
        set_color(read_color());//set_brightness(read_brightness());
    }
}

byte read_turn() {
   return is_turn_on;//if (analogRead(PIN_Brightness) > 0) {return 0;} else {return 1;}
}

byte set_demo(byte turn) {
    byte demo_speed = 0;
    EEPROM.get(ee_addr_start_demo_speed, demo_speed);
  
    if (turn == 1 and ticker_demo.active() == 0) ticker_demo.attach_ms(demo_speed, do_demo);
    if (turn == 0 and ticker_demo.active() == 1) ticker_demo.detach();

    if (turn  == 1) {
        analogWrite(PIN_R, 0);
        analogWrite(PIN_G, 0);
        analogWrite(PIN_B, 127);
    }

    is_demo = turn;
    EEPROM.put(ee_addr_start_demo, is_demo);
    EEPROM.commit();
}

byte read_demo() {
    byte is_demo;
    EEPROM.get(ee_addr_start_demo, is_demo);
    return is_demo;
}

void do_demo() {
    if (read_turn() == 0) return;

    /*set_color(colors[demo_current_color_index]);
    demo_current_color_index += 1;
    if (demo_current_color_index >= 7) demo_current_color_index = 0;*/

    if (demo_stps[0] == 0) { // fade от голубого к фиолетовому
        analogWrite(PIN_R, demo_stps[1]);
    } else  if (demo_stps[0] == 1) { // fade от фиолетового к красному
        analogWrite(PIN_B, demo_stps[1]);
    } else  if (demo_stps[0] == 2) { // fade от красного к желтому
        analogWrite(PIN_G, demo_stps[1]);
    } else  if (demo_stps[0] == 3) { // fade от желтого к зеленому
        analogWrite(PIN_R, demo_stps[1]);
    } else  if (demo_stps[0] == 4) { // fade от зеленого к зеленовато-голубому
        analogWrite(PIN_B, demo_stps[1]);
    } else  if (demo_stps[0] == 5) { // fade от зеленовато-голубого к голубому
        analogWrite(PIN_G, demo_stps[1]);
    }

    if (demo_stps[0] % 2 == 0) demo_stps[1] += 1;
    else demo_stps[1] -= 1;

    if (demo_stps[1] == 0 || demo_stps[1] == 255) {
        demo_stps[0] += 1;
        if (demo_stps[0] > 5) demo_stps[0] = 0;
        Serial.println(String(demo_stps[0])+"   "+demo_stps[1]);
    }
   
}

void statistic_update(void) {

    // время работы
  
    unsigned long t = millis()/1000;
    byte h = t/3600;
    byte m = (t-h*3600)/60;
    unsigned int s = (t-h*3600-m*60);

    stat.time_h = (h>9?"":"0") + String(h, DEC);
    stat.time_m = (m>9?"":"0") + String(m, DEC);
    stat.time_s = (s>9?"":"0") + String(s, DEC);

    // напряжение

    stat.vcc = ((float)(ESP.getVcc()))/20000;
}


void apiHandler() {

    String action = webServer.arg("action");

    DynamicJsonBuffer jsonBuffer;
    JsonObject& answer = jsonBuffer.createObject();
    answer["success"] = 1;
    #if defined(LANG_RU)
        answer["message"] = "Успешно!";
    #elif defined(LANG_EN)
         answer["message"] = "Success!";
    #endif
    JsonObject& data = answer.createNestedObject("data");

    if (action == "set_color") {

        //char _color[7];
        //webServer.arg("color").substring(1).toCharArray(_color, sizeof(_color));

        //const char *_color = webServer.arg("color").substring(1).c_str();
        //unsigned int color = (unsigned int) strtol(_color, NULL, 16);
        unsigned int color = (unsigned int) strtol(webServer.arg("color").substring(1).c_str(), NULL, 16);
        if (read_turn()) set_color(color);
        save_color(color);

        data["color"] = read_color();
        #if defined(LANG_RU)
            answer["message"] = "Цвет изменён!";
        #elif defined(LANG_EN)
            answer["message"] = "The color was changed!";
        #endif

    } else if (action == "set_brightness") {

        byte brightness = webServer.arg("brightness").toInt();
        if (read_turn()) set_brightness(brightness);
        save_brightness(brightness);

        data["brightness"] = read_brightness();
        #if defined(LANG_RU)
            answer["message"] = "Яркость изменена!";
        #elif defined(LANG_EN)
            answer["message"] = "The brightness was changed!";
        #endif

    } else if (action == "turn") {

        byte t = webServer.arg("turn").toInt();

        turn(t);

        if (t == 0) {
             #if defined(LANG_RU)
                answer["message"] = "Выключено!";
             #elif defined(LANG_EN)
                answer["message"] = "Turned off!";
            #endif
        } else {
             #if defined(LANG_RU)
                answer["message"] = "Включено!";
             #elif defined(LANG_EN)
                answer["message"] = "Turned on!";
            #endif
        }

    } else if (action == "demo") {

        byte t = webServer.arg("turn").toInt();

        set_demo(t);
        if (t == 0) set_color(read_color());

        if (t == 0) {
             #if defined(LANG_RU)
                answer["message"] = "Демонстрация выключена!";
             #elif defined(LANG_EN)
                answer["message"] = "Demo has been turned off!";
            #endif
        } else {
             #if defined(LANG_RU)
                answer["message"] = "Демонстрация  включена!";
             #elif defined(LANG_EN)
                answer["message"] = "Demo has been turned on!";
            #endif
        }

    } else if (action == "demo_speed") {

        byte demo_speed = webServer.arg("speed").toInt();

        EEPROM.put(ee_addr_start_demo_speed, demo_speed);
        EEPROM.commit();

        EEPROM.get(ee_addr_start_demo_speed, demo_speed);

        if (read_demo()) {
            set_demo(0);
            set_demo(1);
        }

        data["speed"] = demo_speed;
         #if defined(LANG_RU)
            answer["message"] = "Скорость изменена!";
         #elif defined(LANG_EN)
            answer["message"] = "Speed has been changed!";
        #endif

    } else if (action == "settings_mode") {

        String mode = webServer.arg("wifi_mode");
        if (mode.toInt() > 2 ||mode.toInt() < 0) mode = "0";
        ee_data.wifi_mode = mode.toInt();

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        data["value"] = ee_data.wifi_mode;
        #if defined(LANG_RU)
            answer["message"] = "Сохранено!";
        #elif defined(LANG_EN)
           answer["message"] = "Saved!";
        #endif

    } else if (action == "settings_device_name") {

        String _device_name = webServer.arg("device_name");
        _device_name.toCharArray(ee_data.device_name, sizeof(ee_data.device_name));

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        #if defined(LANG_RU)
            answer["message"] = "Сохранено!";
        #elif defined(LANG_EN)
           answer["message"] = "Saved!";
        #endif

    } else if (action == "settings_other") {

        String _update_time = webServer.arg("update_time");
        ee_data.update_time = _update_time.toInt();

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        #if defined(LANG_RU)
            answer["message"] = "Сохранено!";
        #elif defined(LANG_EN)
           answer["message"] = "Saved!";
        #endif

    } else if (action == "settings") {

        String _ssid = webServer.arg("ssidAP");
        String _password = webServer.arg("passwordAP");
        String _ssidAP = webServer.arg("ssid");
        String _passwordAP = webServer.arg("password");
        _ssid.toCharArray(ee_data.ssidAP, sizeof(ee_data.ssidAP));
        _password.toCharArray(ee_data.passwordAP, sizeof(ee_data.passwordAP));
        _ssidAP.toCharArray(ee_data.ssid, sizeof(ee_data.ssid));
        _passwordAP.toCharArray(ee_data.password, sizeof(ee_data.password));

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        #if defined(LANG_RU)
            answer["message"] = "Сохранено!";
        #elif defined(LANG_EN)
           answer["message"] = "Saved!";
        #endif

    } else if (action == "get_data") {

        String data_type = webServer.arg("data_type");

        if (data_type == "managing" || data_type == "all") {

            data["color"] = read_color();
            data["brightness"] = read_brightness();
            data["turn"] = read_turn();
            data["demo"] = read_demo();

            byte demo_speed = 0;
            EEPROM.get(ee_addr_start_demo_speed, demo_speed);
            data["demo_speed"] = demo_speed;

            JsonObject& _stat = data.createNestedObject("stat");
            statistic_update();
            _stat["vcc"] = stat.vcc;
            _stat["time_h"] = stat.time_h;
            _stat["time_m"] = stat.time_m;
            _stat["time_s"] = stat.time_s;
        }

        if (data_type == "set" || data_type == "all") {

            JsonObject& _settings = data.createNestedObject("settings");
            _settings["wifi_mode"] = ee_data.wifi_mode;
            _settings["password"] = ee_data.password;
            _settings["ssidAP"] = ee_data.ssidAP;
            _settings["passwordAP"] = ee_data.passwordAP;
            _settings["ssid"] = ee_data.ssid;
            _settings["device_name"] = ee_data.device_name;
            _settings["update_time"] = ee_data.update_time;

            JsonObject& _stat = data.createNestedObject("stat");

        }
        data["update_time"] = ee_data.update_time; // для того, чтобы изменение этого значения сразу вступили в силу
        data["device_type"] = DEVICE_TYPE;

        #if defined(LANG_RU)
            answer["message"] = "Информация на странице обновлена";
        #elif defined(LANG_EN)
           answer["message"] = "Infoirmation was updated on the page!";
        #endif

    } else if (action == "settings_reboot") {
        restart();
    } else if (action == "settings_reset") {
        reset_settings();
    } else {
        answer["success"] = 0;
        #if defined(LANG_RU)
            answer["message"] = "неверный API";
        #elif defined(LANG_EN)
           answer["message"] = "Unknown API!";
        #endif
    }

    String sAnswer;
    answer.printTo(sAnswer);
    webServer.send(200, "text/html", sAnswer);
}

void restart() {
    EEPROM.get(ee_addr_start_settings, ee_data);
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    ESP.restart();
}

void reset_settings() {

    // мигаем светодиодом
    byte y = 0;
    for(byte x = 0; x < 10; x++) {
        y = ~y;
        digitalWrite(2, y);
        delay(250);
    }

    EEPROM.write(ee_addr_start_firstrun, 0);
    EEPROM.commit();
    
    restart();

}

// сброс настроек по нажатию на кнопку
void reset_settings_btn() {
    for(byte x = 0; x < 50; x++) {
        delay(100);
        if (digitalRead(pin_btn_reset)==0) return;
    }
    // Настройки сбросятся только если кнопка была зажата в тнчение 5-ти секунд
    reset_settings();
}

/*
 * argument byte address - 7 bit, without RW bit.
 * argument int value - for PCF8575 - two bytes (for ports P00-P07 and P10-P17)
 */
/*void _i2c_channel_write(byte address, byte value[]) {
  Wire.beginTransmission(address);
  Wire.write(value, 2);
  Wire.endTransmission();
}*/

byte wfr_wifiClient_start(byte trying_count) {
    WiFi.begin(ee_data.ssid, ee_data.password);

    byte x = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        x += 1;
        if (x == trying_count) break;
    }

    if (WiFi.status() != WL_CONNECTED) return 0;
    else return 1;
}

void setup() {

    /* Первичная инициализация */

    digitalWrite(2, HIGH);
  
    Serial.begin(115200);
    delay(10);
    EEPROM.begin(4096);//EEPROM.begin(ee_addr_start_settings+sizeof(ee_data));
    delay(10);
    pinMode(pin_btn_reset, INPUT);
    Serial.println();

    pinMode(pin_btn_reset, INPUT);
    // https://mirrobo.ru/c-arduino-ide-esp8266-preryvaniya-interrupt/
    attachInterrupt(pin_btn_reset, reset_settings_btn, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_RIGHT), select_next_color, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_LEFT), select_prev_color, FALLING);
    //attachInterrupt(PIN_BTN_RIGHT, f, FALLING);
    analogWriteRange(255);
    turn(1);
    set_color(read_color());

    /* Инициализация настроек */

    if (EEPROM.read(ee_addr_start_firstrun) != code_firstrun) { // Устанавливаем настройки по умолчанию (если изделие запущено первый раз или настройки быв сброшены пользователем)
        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.put(ee_addr_start_color, 0x0000aaff);
        EEPROM.write(ee_addr_start_demo, 1);
        EEPROM.write(ee_addr_start_demo_speed, 5);
        EEPROM.write(ee_addr_start_firstrun, code_firstrun); // при презапуске устройства код в этих скобках уже не выполнится, если вновь не сбросить натсройки
        EEPROM.commit();
    }

    EEPROM.get(ee_addr_start_settings, ee_data);

    /* подготовка к запуску wifi */

    Serial.println("");
    byte _wifi_mode = ee_data.wifi_mode; // Не трогаем исходное значение

    /* WiFi как клиент */

    if (_wifi_mode == 1 || _wifi_mode == 2) {

        /*WiFi.begin(ee_data.ssid, ee_data.password);

        byte x = 0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            x += 1;
            if (x == 20) break;
        }*/

        is_wifi_client_connected = wfr_wifiClient_start(20);
    
        if (is_wifi_client_connected == 1) {
            Serial.println("WiFi connected");
            Serial.print("Client IP: ");
            Serial.println(WiFi.localIP());
        } else { // если не подключились в качестве клиента, то запускаемся в качестве точки доступа
            WiFi.disconnect(true);
            _wifi_mode = 0;
        }

    }

    /* WiFi как точка доступа */

    if (_wifi_mode == 0 || _wifi_mode == 2) {

        WiFi.softAP(ee_data.ssidAP, ee_data.passwordAP);
      
        Serial.println("AP started");
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());

    }

    /* Запускаем веб-сервер */

    webServer.on("/", HTTP_GET, handler_index_html);
    webServer.on("/api.c", HTTP_POST, apiHandler);
    set_handlers();

    webServer.onNotFound(notFoundHandler);
    webServer.begin();
    //server.begin();
    Serial.println("Server started");

    /* Tickers */

    if (read_demo()) set_demo(1);

}

void loop() {

    webServer.handleClient();

    // если мы не смогли подключиться к сети при старте - пробуем ещё
    if (is_wifi_client_connected == 0 && (ee_data.wifi_mode == 1 || ee_data.wifi_mode == 2)) {
        is_wifi_client_connected = wfr_wifiClient_start(8);
        // если мы подключились к сети, но точку доступа включать не планировали, то выключим её
        if (is_wifi_client_connected == 1 && ee_data.wifi_mode == 1) {
            WiFi.softAPdisconnect();
        }
    }
}

