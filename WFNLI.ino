/*
 * @author: Polyakov Konstantin
 * 
 */



#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ArduinoJson.h>

#define PIN_R 1
#define PIN_G 1
#define PIN_B 1
#define PIN_Brightness 1

ESP8266WebServer webServer(80);

byte pin_btn_reset = 14;

// состояния каналов реле мы не храним в структуре, а отдельно,
// чтобы при каждом изменении сотсояния не трогать настройки.
struct DefaultSettings {
    byte wifi_mode = 0; // 0 - точка доступа, 1 - клиент, 2 - оба варианта одновременно

    char ssidAP[32] = "WiFi_Relay";
    char passwordAP[32] = "";

    char ssid[32] = "Your name";
    char password[32] = "Your password";

    char device_name[64] = "Device name";

    unsigned int update_time = 2000;
};

// eeprom addresses
const unsigned int ee_addr_start_firstrun = 0;
const unsigned int ee_addr_start_color = 1; // Red Green Blue Brightness
const unsigned int ee_addr_start_settings = 5;

const byte code_firstrun = 2;

DefaultSettings ee_data;

byte is_wifi_client_connected = 0;


void notFoundHandler() {
    webServer.send(404, "text/html", "<h1>Not found :-(</h1>");
}

void apiHandler() {

    String action = webServer.arg("action");

    DynamicJsonBuffer jsonBuffer;
    JsonObject& answer = jsonBuffer.createObject();
    answer["success"] = 1;
    answer["message"] = "Успешно!";
    JsonObject& data = answer.createNestedObject("data");

    if (action == "gpio") {

        String msg_d, msg_t;

        int channel = webServer.arg("channel").toInt();
        int value = webServer.arg("value").toInt();
        int sec_timer = webServer.arg("timer").toInt();
        int sec_delay_press = webServer.arg("delay_press").toInt();

        if (sec_timer != 0) {
            int cur_value = wfr_channels.read(channel);
            timers[channel].once(sec_timer + sec_delay_press, Timer_channel_write, (channel<<8) + cur_value);
            msg_t = "через "+ String(sec_timer, DEC) +" сек.";
        }
        if (sec_delay_press != 0) {
            msg_d = " через "+ String(sec_delay_press, DEC) +" сек. будет";
            delay_press[channel].once(sec_delay_press, Timer_channel_write, (channel<<8) + value);
        } else {
            wfr_channels.write(channel, value);
        }

        value = wfr_channels.read(channel);
        data["value"] = value;

        if (value == 1) {
          if (sec_timer != 0) msg_t = " Включится " + msg_t;
          answer["message"] = "Канал " +String(channel+1, DEC)+ msg_d + " включён!" + msg_t;
        } else {
          if (sec_timer != 0) msg_t = " Выключится " + msg_t;
          answer["message"] = "Канал " +String(channel+1, DEC)+ msg_d + " выключен!" + msg_t;
        }

    } else if (action == "led") {
          
        byte value = webServer.arg("value").toInt();
        digitalWrite(2, value==1?0:1);

        value = digitalRead(2)==1?0:1;
        data["value"] = value;

        if (value == 1) answer["message"] = "LED включён!";
        else answer["message"] = "LED выключен!";
            
    } else if (action == "settings_mode") {

        String mode = webServer.arg("wifi_mode");
        if (mode.toInt() > 2 ||mode.toInt() < 0) mode = "0";
        ee_data.wifi_mode = mode.toInt();

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        data["value"] = ee_data.wifi_mode;
        answer["message"] = "Сохранено!";

    } else if (action == "settings_device_name") {

        String _device_name = webServer.arg("device_name");
        _device_name.toCharArray(ee_data.device_name, sizeof(ee_data.device_name));

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        answer["message"] = "Сохранено!";

    } else if (action == "settings_other") {

        String _update_time = webServer.arg("update_time");
        ee_data.update_time = _update_time.toInt();

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        answer["message"] = "Сохранено!";

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

        answer["message"] = "Сохранено!";

    } else if (action == "settings_time") {

        String _date = webServer.arg("date");
        String _time = webServer.arg("time");
        String source = webServer.arg("source");

        if (source == "ntp") {
        } else if (source == "hand" || source == "browser") {

            //setSyncProvider(RTC.get);   // получаем время с RTC
            //if (timeStatus() != timeSet)
            //    Serial.println("Unable to sync with the RTC"); //синхронизация не удаласть
            //else
            //    Serial.println("RTC has set the system time");
            TimeElements te;
            te.Second = source=="browser" ? webServer.arg("seconds").toInt() : 0;
            te.Minute = _time.substring(3, 5).toInt();
            te.Hour = _time.substring(0, 2).toInt();
            te.Day = _date.substring(8, 10).toInt();
            te.Month = _date.substring(5, 7).toInt();
            te.Year = _date.substring(0, 4).toInt() - 1970; //год в библиотеке отсчитывается с 1970
            time_t timeVal = makeTime(te);
            RTC.set(timeVal);
            setTime(timeVal);
        }

        answer["message"] = "Время обновлено!";

    } else if (action == "get_data") {

        String data_type = webServer.arg("data_type");

        if (data_type == "std" || data_type == "all") {

            data["count_outlets"] = COUNT_OUTLETS;

            JsonObject& _stat = data.createNestedObject("stat");
            statistic_update();
            _stat["vcc"] = stat.vcc;
            _stat["time_h"] = stat.time_h;
            _stat["time_m"] = stat.time_m;
            _stat["time_s"] = stat.time_s;

            _stat["rtc_h"] = stat.rtc_h;
            _stat["rtc_m"] = stat.rtc_m;
            _stat["rtc_s"] = stat.rtc_s;
            _stat["rtc_day"] = stat.rtc_day;
            _stat["rtc_month"] = stat.rtc_month;
            _stat["rtc_year"] = stat.rtc_year;
            _stat["rtc_is"] = stat.rtc_is;
            //data.parseObject();
            //JsonObject& _settings = settings.parseObject(ee_data);

        }
        
        if (data_type == "btn" || data_type == "all") {

            data["bt_panel"] = ee_data.bt_panel;
            data["max_size"] = BT_PANEL_SIZE;

        }

        if (data_type == "btn" || data_type == "std" || data_type == "all") {

            data["gpio_std"] = wfr_channels.read_all();
            data["gpio_led"] = digitalRead(2)==1?0:1;
            
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
            statistic_update();

            _stat["rtc_h"] = stat.rtc_h;
            _stat["rtc_m"] = stat.rtc_m;
            _stat["rtc_s"] = stat.rtc_s;
            _stat["rtc_day"] = stat.rtc_day;
            _stat["rtc_month"] = stat.rtc_month;
            _stat["rtc_year"] = stat.rtc_year;
            _stat["rtc_is"] = stat.rtc_is;

        }
        data["update_time"] = ee_data.update_time; // для того, чтобы изменение этого значения сразу вступили в силу

        answer["message"] = "Информация на странице обновлена";

    } else if (action == "bt_panel_save") {

        webServer.arg("bt_panel").toCharArray(ee_data.bt_panel, sizeof(ee_data.bt_panel));

        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.commit();

        //bt_panel = webServer.arg("bt_panel");
        //webServer.arg("bt_panel").toCharArray(bt_panel, sizeof(bt_panel));
        //EEPROM.put(ee_addr_start_bt_panel, bt_panel);

        answer["message"] = "Панель сохранена";

    } else if (action == "settings_reboot") {
        restart();
    } else if (action == "settings_reset") {
        reset_settings();
    } else {
        answer["success"] = 0;
        answer["message"] = "неверный API";
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
  
    Serial.begin(115200);
    delay(10);
    EEPROM.begin(4096);//EEPROM.begin(ee_addr_start_settings+sizeof(ee_data));
    delay(10);
    pinMode(pin_btn_reset, INPUT);
    Serial.println();

    pinMode(pin_btn_reset, INPUT);
    attachInterrupt(pin_btn_reset, reset_settings_btn, RISING);

    /* Инициализация настроек */

    if (EEPROM.read(ee_addr_start_firstrun) != code_firstrun) { // Устанавливаем настройки по умолчанию (если изделие запущено первый раз или настройки быв сброшены пользователем)
        EEPROM.put(ee_addr_start_settings, ee_data);
        EEPROM.put(ee_addr_start_color, 0x0000aaff);
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

