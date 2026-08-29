#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_log.h"
#include "device.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_timer.h"
#include "esp_littlefs.h"

static const char *TAG = "DEVICE_SETTINGS";


#define DEFAULT_VREF    1100        
#define NO_OF_SAMPLES   32         

gpio_num_t button_pins[] = {
    PAGE_LEFT_BUTTON, ARROW_LEFT_BUTTON, MIDDLE_BUTTON, ARROW_UP_BUTTON, ARROW_DOWN_BUTTON, ARROW_RIGHT_BUTTON, PAGE_RIGHT_BUTTON
};


static esp_adc_cal_characteristics_t adc2_chars;

typedef struct {
    float voltage;
    int percent;
} batt_table_t;

static const batt_table_t batt_table[] = {
    {4.20, 100},
    {4.10,  90},
    {3.95,  75},
    {3.85,  50},
    {3.70,  25},
    {3.55,  0},
};

void Device::saveSettings() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE("Device", "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    // Build JSON
    cJSON *root = cJSON_CreateObject();

    // Render settings
    cJSON *render = cJSON_CreateObject();
    cJSON_AddNumberToObject(render, "marginsHorizontal", renderSettings.marginsHorizontal);
    cJSON_AddNumberToObject(render, "marginsVertical", renderSettings.marginsVertical);
    cJSON_AddNumberToObject(render, "fontSize", renderSettings.fontSize);
    cJSON_AddNumberToObject(render, "fontBold", renderSettings.fontBold);
    cJSON_AddNumberToObject(render, "fontPoints", renderSettings.fontPoints);
    cJSON_AddNumberToObject(render, "lineSpacing", renderSettings.lineSpacing);
    cJSON_AddStringToObject(render, "fontName", renderer->fontHandler.families[renderSettings.fontFamily].name.c_str());
    cJSON_AddItemToObject(root, "renderSettings", render);

    // Device settings
    cJSON *device = cJSON_CreateObject();
    cJSON_AddNumberToObject(device, "displayRefresh", deviceSettings.displayRefresh);
    cJSON_AddNumberToObject(device, "buzzerEnabled", deviceSettings.buzzerEnabled);
    cJSON_AddNumberToObject(device, "buzzerIntensity", deviceSettings.buzzerIntensity);
    cJSON_AddNumberToObject(device, "nightMode", deviceSettings.nightMode);
    cJSON_AddNumberToObject(device, "displayBattery", deviceSettings.displayBattery);
    cJSON_AddNumberToObject(device, "standbyTimeout", deviceSettings.standbyTimeout);
    cJSON_AddNumberToObject(device, "standbyScreen", deviceSettings.standbyScreen);
    cJSON_AddNumberToObject(device, "sunlightMode", deviceSettings.sunlightMode);
    cJSON_AddNumberToObject(device, "showPagePercentage", deviceSettings.showPagePercentage);
    cJSON_AddNumberToObject(device, "storeDataOnSD", deviceSettings.storeDataOnSD);
    cJSON_AddNumberToObject(device, "smartImageDetect", deviceSettings.smartImageDetect);
    cJSON_AddNumberToObject(device, "standbyShutdown", deviceSettings.standbyShutdown);
    cJSON_AddNumberToObject(device, "sunlightFullRefresh", deviceSettings.sunlightFullRefresh);
    cJSON_AddNumberToObject(device, "vcomLeft", deviceSettings.vcomLeft);
    cJSON_AddNumberToObject(device, "vcomRight", deviceSettings.vcomRight);
    cJSON_AddItemToObject(root, "deviceSettings", device);

    

    char *jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr) {
        ESP_LOGI("Device", "Saving settings: %s", jsonStr);
        ESP_ERROR_CHECK(nvs_set_blob(nvs, "settings", jsonStr, strlen(jsonStr)));
        cJSON_free(jsonStr);
    }

    cJSON_Delete(root);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void Device::loadSettings() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW("Device", "No settings found in NVS, using defaults");
        //load espy sans as a default font
        renderSettings.fontPoints = 16;
        std::string fontName = "Espy Sans";
        renderSettings.fontFamily  = 0;
        for(int i=0;i<renderer->fontHandler.families.size();i++)
        {
            if(fontName==renderer->fontHandler.families[i].name) renderSettings.fontFamily = i;

        }
        return;
    }

    size_t len = 0;
    if (nvs_get_blob(nvs, "settings", NULL, &len) != ESP_OK || len == 0) {
        ESP_LOGW("Device", "Settings blob missing, using defaults");
        nvs_close(nvs);
        return;
    }

    std::vector<char> buffer(len + 1, 0);
    if (nvs_get_blob(nvs, "settings", buffer.data(), &len) == ESP_OK) {
        ESP_LOGI("Device", "Loaded settings JSON: %s", buffer.data());
        cJSON *root = cJSON_Parse(buffer.data());
        if (root) {
            cJSON *j;

            // Render settings
            cJSON *render = cJSON_GetObjectItem(root, "renderSettings");
            if (render && cJSON_IsObject(render)) {
                if ((j = cJSON_GetObjectItem(render, "marginsHorizontal")) && cJSON_IsNumber(j))
                    renderSettings.marginsHorizontal = j->valueint;
                if ((j = cJSON_GetObjectItem(render, "marginsVertical")) && cJSON_IsNumber(j))
                    renderSettings.marginsVertical = j->valueint;
                if ((j = cJSON_GetObjectItem(render, "fontSize")) && cJSON_IsNumber(j))
                    renderSettings.fontSize = j->valueint;
                if ((j = cJSON_GetObjectItem(render, "fontBold")) && cJSON_IsNumber(j))
                    renderSettings.fontBold = j->valueint;
                if ((j = cJSON_GetObjectItem(render, "fontPoints")) && cJSON_IsNumber(j))
                    renderSettings.fontPoints = j->valueint;
                if ((j = cJSON_GetObjectItem(render, "lineSpacing")) && cJSON_IsNumber(j))
                    renderSettings.lineSpacing = j->valueint;
                if ((j = cJSON_GetObjectItem(render, "fontName")) && cJSON_IsString(j))
                {
                    std::string fontName = j->valuestring;
                    renderSettings.fontFamily  = 0;
                    for(int i=0;i<renderer->fontHandler.families.size();i++)
                    {
                        if(fontName==renderer->fontHandler.families[i].name) renderSettings.fontFamily = i;

                    }
                    
                }

            }

            // Device settings
            cJSON *device = cJSON_GetObjectItem(root, "deviceSettings");
            if (device && cJSON_IsObject(device)) {
                if ((j = cJSON_GetObjectItem(device, "displayRefresh")) && cJSON_IsNumber(j))
                    deviceSettings.displayRefresh = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "buzzerEnabled")) && cJSON_IsNumber(j))
                    deviceSettings.buzzerEnabled = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "buzzerIntensity")) && cJSON_IsNumber(j))
                    deviceSettings.buzzerIntensity = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "nightMode")) && cJSON_IsNumber(j))
                    deviceSettings.nightMode = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "displayBattery")) && cJSON_IsNumber(j))
                    deviceSettings.displayBattery = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "standbyTimeout")) && cJSON_IsNumber(j))
                    deviceSettings.standbyTimeout= j->valueint;
                if ((j = cJSON_GetObjectItem(device, "standbyScreen")) && cJSON_IsNumber(j))
                    deviceSettings.standbyScreen = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "sunlightMode")) && cJSON_IsNumber(j))
                    deviceSettings.sunlightMode = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "showPagePercentage")) && cJSON_IsNumber(j))
                    deviceSettings.showPagePercentage = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "storeDataOnSD")) && cJSON_IsNumber(j))
                    deviceSettings.storeDataOnSD = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "smartImageDetect")) && cJSON_IsNumber(j))
                    deviceSettings.smartImageDetect = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "standbyShutdown")) && cJSON_IsNumber(j))
                    deviceSettings.standbyShutdown = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "sunlightFullRefresh")) && cJSON_IsNumber(j))
                    deviceSettings.sunlightFullRefresh = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "vcomLeft")) && cJSON_IsNumber(j))
                    deviceSettings.vcomLeft = j->valueint;
                if ((j = cJSON_GetObjectItem(device, "vcomRight")) && cJSON_IsNumber(j))
                    deviceSettings.vcomRight = j->valueint;
                }

            cJSON_Delete(root);
        } else {
            ESP_LOGW("Device", "Failed to parse settings JSON, using defaults");
        }
    }

    nvs_close(nvs);
}

void Device::loadFont()
{
    for(int i=0;i<renderer->fontHandler.families[renderSettings.fontFamily].fonts.size();i++)
    {
        if(renderer->fontHandler.families[renderSettings.fontFamily].fonts[i].pointSize==renderSettings.fontPoints)
        {
            renderer->fontHandler.loadFont(renderer->fontHandler.families[renderSettings.fontFamily].fonts[i].fileName);
            return;
        }


    }
    renderer->fontHandler.loadFont(renderer->fontHandler.families[0].fonts[0].fileName); //default to unifont
    return;
}

void Device::initADC()
{
    adc2_config_channel_atten(ADC2_CHANNEL_2, ADC_ATTEN_DB_11);

    // Characterize ADC
    esp_adc_cal_characterize(ADC_UNIT_2,
                             ADC_ATTEN_DB_11,
                             ADC_WIDTH_BIT_12,
                             DEFAULT_VREF,
                             &adc2_chars);
}

// float Device::getBatteryVoltage()
// {
//     int raw = 0;
//     int sum = 0;

//     // Take multiple samples
//     for (int i = 0; i < NO_OF_SAMPLES; i++) {
//         int val;
//         if (adc2_get_raw(ADC2_CHANNEL_2, ADC_WIDTH_BIT_12, &val) == ESP_OK) {
//             sum += val;
//         }
//     }
//     raw = sum / NO_OF_SAMPLES;

//     // Convert raw reading to voltage (mV at ADC pin)
//     uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc2_chars);

//     // Scale up for divider (100k / 100k = ×2)
//     return (voltage_mv / 1000.0f) * 2.0f;
// }

float Device::getBatteryVoltage()
{
    int raw = 0;
    int sum = 0;

    gpio_hold_dis(B_SENSE_CTRL);
    gpio_set_level(B_SENSE_CTRL, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    // Take multiple samples
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        int val;
        if (adc2_get_raw(ADC2_CHANNEL_2, ADC_WIDTH_BIT_12, &val) == ESP_OK) {
            sum += val;
        }
    }
    raw = sum / NO_OF_SAMPLES;

    // Convert raw reading to voltage (mV at ADC pin)
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc2_chars);

    // Scale up for divider (100k / 100k = ×2)
    gpio_set_level(B_SENSE_CTRL, 0);
    gpio_hold_en(B_SENSE_CTRL);
    return (voltage_mv / 1000.0f) * 2.0f;
}

int Device::getBatteryPercentage()
{
    float vbat = this->getInstance().getBatteryVoltage();
    for (int i = 0; i < sizeof(batt_table)/sizeof(batt_table[0]) - 1; i++) {
        if (vbat >= batt_table[i+1].voltage) {
            float v1 = batt_table[i].voltage;
            float v2 = batt_table[i+1].voltage;
            int p1 = batt_table[i].percent;
            int p2 = batt_table[i+1].percent;

            // linear interpolation
            return p1 + (int)((vbat - v1) * (p2 - p1) / (v2 - v1));
        }
    }
    return 0; // below lowest entry
}

void Device::configureButtons()
{
    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << button_pins[i],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
    }

    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_wakeup_enable(button_pins[i],GPIO_INTR_LOW_LEVEL);
    }

}

void Device::clearButtonLatches()
{
    ESP_LOGI("Device", "Clearing all latches");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        buttonLatchedStates[button_pins[i]]=false;
    }
}

void Device::pollButtons()
{
    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool buttonPress = false;
        if(!gpio_get_level(button_pins[i]) && !buttonStates[button_pins[i]]) buttonPress=true;
        buttonStates[button_pins[i]] = !gpio_get_level(button_pins[i]);
        if(buttonPress && int(esp_timer_get_time())>this->latchTimeOut)
        {
            //if(buttonLatchedStates[button_pins[i]]==false) buzz();
            
            buttonLatchedStates[button_pins[i]]=true;

            ESP_LOGI("Device", "Button press detected: %d: ", button_pins[i]);
            ESP_LOGI("Device", "current Time: %d", int(esp_timer_get_time()));
            ESP_LOGI("Device", "set Time: %d", latchTimeOut);
            buzz();
            ESP_LOGI("Device", "Current latch state: %d",buttonLatchedStates[button_pins[i]]);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}


void Device::setLatchTimeOut(int duration)
{
    ESP_LOGI("Device", "Setting Latch Timeout");
    this->latchTimeOut = int(esp_timer_get_time()) + duration;
    ESP_LOGI("Device", "current Time: %d", int(esp_timer_get_time()));
    ESP_LOGI("Device", "set Time: %d", latchTimeOut);
}

void Device::displayOffScreen(std::string message)
{
    //when entering deep sleep, there are 3 possible idle screens. 

    //Idle screen 1: 2 images on both pages
    FILE *f_left = fopen("/sdcard/idle_screen_left.jpg", "r");
    FILE *f_right = fopen("/sdcard/idle_screen_right.jpg", "r");
    if (Device::getInstance().deviceSettings.standbyScreen==2 && f_left && f_right) {
        fclose(f_left);
        fclose(f_right);
        renderer->framebuffer = menuHandler->leftPageFrameBuffer;
        Image left_image = Image("/sdcard/idle_screen_left.jpg","");
        left_image.prepare();
        left_image.decodeAndScale(EPD_HEIGHT,EPD_WIDTH);
        left_image.floydSteinbergDither();
        renderer->drawImage(left_image,0,0);
        
        renderer->drawSquare(0,0,(message.length()+2)*GLYPH_HEIGHT/2,2*GLYPH_WIDTH,false);
        renderer->drawString(GLYPH_HEIGHT/2,GLYPH_WIDTH/2,message,1,true,false,false);

        renderer->framebuffer = menuHandler->rightPageFrameBuffer;
        Image right_image = Image("/sdcard/idle_screen_right.jpg","");
        right_image.prepare();
        right_image.decodeAndScale(EPD_HEIGHT,EPD_WIDTH);
        right_image.floydSteinbergDither();
        renderer->drawImage(right_image,0,0);
        
        renderer->epd.forceRefresh();
        renderer->epd.DisplayPictureBoth(menuHandler->leftPageFrameBuffer,menuHandler->rightPageFrameBuffer);
    } 
    else if(Device::getInstance().deviceSettings.standbyScreen==1 && Device::getInstance().state==Device::State::Reading) //Idle screen 2: the cover of the current book
    {
        ESP_LOGI(TAG, "data: %s",reader->epub->get_cover_image_item().c_str());
        ESP_LOGI(TAG, "data: %s",reader->epub->get_path().c_str());
        renderer->framebuffer = reader->leftPageFrameBuffer; //set the renderer framebuffer to the current left page, just in case the cover can't be loaded
        Image cover_image = Image(reader->epub->get_cover_image_item(),reader->epub->get_path());
        cover_image.prepare();
        cover_image.decodeAndScale(EPD_HEIGHT,EPD_WIDTH);
        cover_image.floydSteinbergDither();
        renderer->drawImage(cover_image,0,0);
        
        renderer->drawSquare(0,0,(message.length()+2)*GLYPH_HEIGHT/2,2*GLYPH_WIDTH,false);
        renderer->drawString(GLYPH_HEIGHT/2,GLYPH_WIDTH/2,message,1,true,false,false);
        renderer->epd.forceRefresh();
        renderer->epd.DisplayPicture(true,renderer->framebuffer);
    }
    else if(Device::getInstance().deviceSettings.standbyScreen==1 && Device::getInstance().state==Device::State::simpleReader) //Idle screen 2: the cover of the current book
    {
        ESP_LOGI(TAG, "data: %s",simpleReader->epub->get_cover_image_item().c_str());
        ESP_LOGI(TAG, "data: %s",simpleReader->epub->get_path().c_str());
        renderer->framebuffer = reader->leftPageFrameBuffer; //set the renderer framebuffer to the current left page, just in case the cover can't be loaded
        Image cover_image = Image(simpleReader->epub->get_cover_image_item(),simpleReader->epub->get_path());
        cover_image.prepare();
        cover_image.decodeAndScale(EPD_HEIGHT,EPD_WIDTH);
        cover_image.floydSteinbergDither();
        renderer->drawImage(cover_image,0,0);
        
        renderer->drawSquare(0,0,(message.length()+2)*GLYPH_HEIGHT/2,2*GLYPH_WIDTH,false);
        renderer->drawString(GLYPH_HEIGHT/2,GLYPH_WIDTH/2,message,1,true,false,false);
        renderer->epd.forceRefresh();
        renderer->epd.DisplayPicture(true,renderer->framebuffer);
    }
    else { //Idle screen 3: a small standby indicator on the bottom left
        renderer->drawSquare(0,0,(message.length()+2)*GLYPH_HEIGHT/2,2*GLYPH_WIDTH,false);
        renderer->drawString(GLYPH_HEIGHT/2,GLYPH_WIDTH/2,message,1,true,false,false);
        renderer->drawScreenPartial(true,0,0,(message.length()+2)*GLYPH_HEIGHT/2,2*GLYPH_WIDTH);
    }
}

void Device::errorBlinkLED(int count)
{
    for(int i=0;i<count;i++)
    {
        vTaskDelay(pdMS_TO_TICKS(300));
        gpio_set_level(GPIO_NUM_48, 0);
        vTaskDelay(pdMS_TO_TICKS(30));
        gpio_set_level(GPIO_NUM_48, 1);
    }
}


void Device::saveAppState() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for app_state: %s", esp_err_to_name(err));
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "activeBookPath", Device::getInstance().activeBookPath.c_str());
    cJSON_AddStringToObject(root, "activeAuthorName", Device::getInstance().activeAuthorName.c_str());
    cJSON_AddNumberToObject(root, "state", (int) Device::getInstance().state);

    char *jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr) {
        ESP_ERROR_CHECK(nvs_set_blob(nvs, "app_state", jsonStr, strlen(jsonStr)));
        cJSON_free(jsonStr);
    }


    

    cJSON_Delete(root);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void Device::loadAppState() {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READONLY, &nvs);
    if (err != ESP_OK) return;

    size_t len = 0;
    if (nvs_get_blob(nvs, "app_state", NULL, &len) != ESP_OK || len == 0) {
        nvs_close(nvs);
        return;
    }

    std::vector<char> buffer(len + 1, 0);
    if (nvs_get_blob(nvs, "app_state", buffer.data(), &len) == ESP_OK) {
        cJSON *root = cJSON_Parse(buffer.data());
        if (root) {
            cJSON *jpath = cJSON_GetObjectItem(root, "activeBookPath");
            cJSON *jauthor = cJSON_GetObjectItem(root, "activeAuthorName");
            cJSON *jstate = cJSON_GetObjectItem(root, "state");

            if (cJSON_IsString(jpath)) {
                Device::getInstance().activeBookPath = jpath->valuestring;
            }
            if (cJSON_IsString(jauthor)) {
                Device::getInstance().activeAuthorName = jauthor->valuestring;
            }
            if (cJSON_IsNumber(jstate)) {
                Device::getInstance().state = (Device::State) jstate->valueint;  
            }

            cJSON_Delete(root);
        }
    }

    nvs_close(nvs);
}