// Device stores global information and variables
#pragma once
#include <string>
#include "reader.h"
#include "simpleReader.h"
#include "bookHandler.h"
#include "notificationHandler.h"
#include "menuHandler.h"
#include "SDCard.h"

#define PAGE_LEFT_BUTTON GPIO_NUM_5
#define ARROW_LEFT_BUTTON GPIO_NUM_1
#define MIDDLE_BUTTON GPIO_NUM_0
#define ARROW_UP_BUTTON GPIO_NUM_2
#define ARROW_DOWN_BUTTON GPIO_NUM_3
#define ARROW_RIGHT_BUTTON GPIO_NUM_4
#define PAGE_RIGHT_BUTTON GPIO_NUM_6
#define USB_POWER_TRIGGER GPIO_NUM_15
#define B_SENSE_CTRL GPIO_NUM_43 

extern gpio_num_t button_pins[];
#define NUM_BUTTONS 7

class Device {
public:
    // Nested struct for global settings
    struct RenderSettings {
        int marginsHorizontal = 1;
        int marginsVertical = 0;
        int fontSize = 1;
        int fontBold = 0;
        int fontFamily = 0;
        int fontPoints = 0;
        int lineSpacing = 0;
    };

    struct DeviceSettings {
        int displayRefresh = 5;
        int buzzerEnabled = 1;
        int buzzerIntensity = 5;
        int nightMode = 0;
        int displayBattery = 1;
        int standbyTimeout = 10;
        int standbyScreen = 1;
        int sunlightMode = 0;
        int showPagePercentage = 0;
        int storeDataOnSD = 0;
        int smartImageDetect = 0;
        int standbyShutdown = 7;
        int sunlightFullRefresh = 0;
        int vcomLeft = 23;
        int vcomRight = 23;
    };

    // Access the singleton instance
    static Device& getInstance() {
        static Device instance;
        return instance;
    }

    // Delete copy/move to ensure single instance
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    // Example state variables
    enum class State {
        Reading,
        Menu,
        USB_FileTransfer,
        simpleReader
        };

    enum class usbState {
        Unconnected,
        Query,
        Charging,
        FileTransfer
    };
    State state = State::Menu;
    usbState usb_state = usbState::Unconnected;

    // Global settings instance
    RenderSettings renderSettings;
    DeviceSettings deviceSettings;
    Reader *reader = nullptr;
    SimpleReader *simpleReader = nullptr;
    MenuHandler *menuHandler = nullptr;
    BookHandler *bookHandler = nullptr;
    Renderer *renderer = nullptr;
    NotificationHandler *notificationHandler = nullptr;
    SDCard *sd = nullptr;

    std::string activeBookPath;
    std::string activeAuthorName;
    int activeBookIndex = -1;
    int activeAuthorIndex = -1;
    volatile int buttonStates[7] = {0};
    volatile int buttonLatchedStates[7] = {0};

    // // Example methods
    // void turnOn() { state = State::Active; }
    // void turnOff() { state = State::Off; }
    // void resetSettings() { settings = Settings(); }
    static void buzz() {
        if(!Device::getInstance().deviceSettings.buzzerEnabled) return;
        gpio_set_level(GPIO_NUM_47, 1);
        vTaskDelay(Device::getInstance().deviceSettings.buzzerIntensity);
        gpio_set_level(GPIO_NUM_47, 0);
    }

    static void dubbleBuzz()
    {
        Device::buzz();
        vTaskDelay(20);
        Device::buzz();
    }

    static void shutdown() {
        gpio_hold_dis(GPIO_NUM_38); //disable the hold on the power pin
        gpio_set_level(GPIO_NUM_38, 0); //set the led and power to low
        gpio_set_level(GPIO_NUM_48, 0);
        vTaskDelay(100000); //and now wait until the user releases the power button
    };
    void saveAppState();
    void loadAppState();
    void errorBlinkLED(int count);
    void displayOffScreen(std::string message);
    void configureButtons();
    void clearButtonLatches();
    void setLatchTimeOut(int duration);
    void pollButtons();
    volatile int latchTimeOut = 0;
    //void pollButtonsLatching();

    int rightButtonState = 1;
    int middleButtonState = 1;
    int leftButtonState = 1;
    int upButtonState = 1;
    int downButtonState = 1;
    int leftPageButtonState = 1;
    int rightPageButtonState = 1;

    void saveSettings();
    void loadSettings();
    void loadFont();
    void initADC();
    float getBatteryVoltage();
    int getBatteryPercentage();

private:
    // Private constructor
    Device() = default;
};