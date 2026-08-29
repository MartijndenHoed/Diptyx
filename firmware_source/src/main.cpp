#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "epd5in83b_V2.h"
#include "contentParser.h"
#include "SDCard.h"
#include <dirent.h>
#include "esp_task_wdt.h"
#include "reader.h"
#include "simpleReader.h"
#include "renderer.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "usbMassStorage.h"
#include "driver/adc.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#include "tinyusb_default_config.h"
#include "driver/rtc_io.h"
#include "menuHandler.h"
#include "device.h"

int64_t last_interaction_time = 0;
int displayTimeOut = 10; //time in seconds before shutting down screens (in menus)
bool safeboot = false;

static const char *TAG = "EPD_MAIN";
static SDCard *sd = nullptr;
BookHandler *bookHandler = nullptr;
MenuHandler *menuHandler = nullptr;
Reader *reader = nullptr;
SimpleReader *simpleReader = nullptr;
NotificationHandler *notificationHandler = nullptr;
Renderer *renderer = nullptr;


void configure_buttons_deep_sleep()
{
    // Configure all buttons as RTC GPIO inputs with pull-ups
    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_num_t pin = button_pins[i];

        // Configure as RTC GPIO input
        rtc_gpio_init(pin);
        rtc_gpio_set_direction(pin, RTC_GPIO_MODE_INPUT_ONLY);

        // Enable pull-up (assuming button pulls to GND when pressed)
        rtc_gpio_pullup_en(pin);
        rtc_gpio_pulldown_dis(pin);
    }

    // Build a mask of all button pins
    uint64_t pin_mask = 0;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pin_mask |= (1ULL << button_pins[i]);
    }

    rtc_gpio_pullup_dis(USB_POWER_TRIGGER);
    rtc_gpio_pulldown_dis(USB_POWER_TRIGGER);
    rtc_gpio_init(USB_POWER_TRIGGER);
    rtc_gpio_set_direction(USB_POWER_TRIGGER, RTC_GPIO_MODE_INPUT_ONLY);
    pin_mask |= (1ULL << USB_POWER_TRIGGER);


    // Configure EXT1 wakeup: wake on ANY button press (goes low)
    esp_sleep_enable_ext1_wakeup(pin_mask, ESP_EXT1_WAKEUP_ANY_LOW);
}



void main_task(void *param) {
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_INPUT); //configure the usb detect pin power button pin
    gpio_pulldown_en(GPIO_NUM_16); 
    gpio_hold_en(GPIO_NUM_16);
    gpio_set_direction(GPIO_NUM_42, GPIO_MODE_INPUT);
    gpio_pulldown_en(GPIO_NUM_42);
    gpio_hold_en(GPIO_NUM_42);

    gpio_pullup_dis(USB_POWER_TRIGGER);
    gpio_pulldown_dis(USB_POWER_TRIGGER);
    gpio_set_direction(USB_POWER_TRIGGER, GPIO_MODE_INPUT);
    gpio_wakeup_enable(USB_POWER_TRIGGER,GPIO_INTR_LOW_LEVEL); //configure the usb/power detect pin

    Device& device = Device::getInstance(); //get the global device object
    device.configureButtons(); //configure the buttons and enable deep sleep wakeup
    esp_sleep_enable_gpio_wakeup();

    renderer = new Renderer(); //start the renderer
    device.renderer = renderer;
    renderer->init();

    sd = new SDCard("/sdcard", GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39); //mount the sd card

    if(sd->mountedSuccesfully) renderer->fontHandler.indexFonts(); //load the fonts

    reader = new Reader();
    device.reader = reader;
    simpleReader = new SimpleReader();
    device.simpleReader = simpleReader;
    renderer->framebuffer = reader->leftPageFrameBuffer; //set it to something, just to be sure
    notificationHandler = new NotificationHandler(renderer);
    device.notificationHandler = notificationHandler;
    device.sd = sd;

    if(!sd->mountedSuccesfully) //check if the sd card is actually loaded before we proceed
    {
        notificationHandler->drawSDcardErrorNotification(); //show possible error and shutdown
        Device::dubbleBuzz();
        Device::shutdown();
    }


    int64_t now = esp_timer_get_time();
    while(gpio_get_level(GPIO_NUM_42) && !gpio_get_level(PAGE_LEFT_BUTTON)) { //check if we need to safeboot
        vTaskDelay(10);
            if(esp_timer_get_time() - now> 3000000) //safeboot if pressed for more than 3 seconds
            {
                notificationHandler->drawNotification("Entering safeboot");
                safeboot = true;
            }
        }

    now = esp_timer_get_time();
    while(gpio_get_level(GPIO_NUM_42) && gpio_get_level(GPIO_NUM_16) && !gpio_get_level(USB_POWER_TRIGGER))  { //check if we need to boot into storage access
            vTaskDelay(10);
            
            if(esp_timer_get_time() - now> 1000000) //boot into storage access if the button is pressed for 1s
            {
                notificationHandler->drawStorageAccessNotication();
                Device::buzz();
                //stop usb cdc and unmount sd card
                tinyusb_console_deinit(TINYUSB_CDC_ACM_0);
                tinyusb_cdcacm_deinit(TINYUSB_CDC_ACM_0);
                tinyusb_driver_uninstall();
                delete(sd);
                vTaskDelay(10);

                ESP_LOGI(TAG, "USB Mass Storage mode");
                if (usb_msc_sdmmc_start(GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39, 1) == ESP_OK) {

                    // Wait until USB cable disconnected (GPIO low)
                    while(gpio_get_level(GPIO_NUM_16)) {
                        vTaskDelay(10);
                    }

                    ESP_LOGI(TAG, "USB disconnected, stopping MSC...");
                    usb_msc_stop();  // stop MSC
                    esp_restart();
                    
                } else {
                    ESP_LOGE(TAG, "USB MSC failed to start");
                }


            }

        }




    now = esp_timer_get_time();
    while(gpio_get_level(GPIO_NUM_42) && !gpio_get_level(GPIO_NUM_16)) { //check if we need to force shutdown
            vTaskDelay(10);
            
            if(esp_timer_get_time() - now> 3000000) //shutdown if pressed for more than 3s
            {
                Device::dubbleBuzz();
                notificationHandler->drawNotification("Shutting down device...");
                Device::shutdown();
            }

        }



    menuHandler =  new MenuHandler(renderer); //populate the handlers
    device.menuHandler = menuHandler;
    bookHandler = new BookHandler();
    device.bookHandler = bookHandler;

    device.loadAppState(); //load the state of the bookhandler
    device.initADC();
    device.loadSettings();
    device.saveSettings();
    if(safeboot) renderer->fontHandler.loadFont(renderer->fontHandler.families[0].fonts[0].fileName); //in safeboot we use unifont only
    else device.loadFont();
    bookHandler->listBooks();
    menuHandler->layoutReadMenu(bookHandler->authorList); //load the book menu structure, and load the current settings values
    menuHandler->updateFontSelect(); //and update the font options in the menu
    menuHandler->updateFontSize(); //and update the font size options
    
    if(safeboot) device.state=Device::State::Menu; 

    
    if(device.state==Device::State::Reading) //Here we check if there is actually a valid book loading
    {
        if(device.activeAuthorIndex==-1 || device.activeBookIndex==-1 || (device.activeAuthorIndex==0 && bookHandler->authorList[0].bookList.size()==0))
        {
            device.state=Device::State::Menu; 
        }
        else
        {
            if(device.activeAuthorIndex>=bookHandler->authorList.size() || device.activeBookIndex >= (bookHandler->authorList[device.activeAuthorIndex]).bookList.size()) //double check if the book is actually accessible with these indices
            {
                device.state=Device::State::Menu; 
            }
            else
            {
                Book *book = (bookHandler->authorList[device.activeAuthorIndex]).bookList[device.activeBookIndex];
                if(book->totalPageCount==0) device.state=Device::State::Menu; 
            }
            
        }
    }


    if(device.state==Device::State::Menu)//boot into the bookmenu
    {
        menuHandler->drawMenu();
    }
    else if(device.state==Device::State::Reading) //or boot into reading
    {
        Book *book = (bookHandler->authorList[device.activeAuthorIndex]).bookList[device.activeBookIndex];
        reader->init(book,renderer);
        // reader->chapterPageCounts = book->chapterPageCounts;
        // reader->totalPages = book->totalPageCount;
        // reader->currentPage = book->readPageCount;
        reader->openPage();
        menuHandler->currentElement = menuHandler->authorMenu->children[device.activeAuthorIndex];
        std::static_pointer_cast<MenuElement> (menuHandler->currentElement)->selectedChildIndex = device.activeBookIndex;
    }
    else if(device.state==Device::State::simpleReader) //or boot into the simplereader
    {
        simpleReader->init(Device::getInstance().activeBookPath,renderer);
    }

    if(gpio_get_level(GPIO_NUM_16) && !gpio_get_level(USB_POWER_TRIGGER) && device.usb_state==Device::usbState::Unconnected && device.state!=Device::State::simpleReader) //check if we're booting with usb connected
    {
        device.usb_state=Device::usbState::Query;
        device.notificationHandler->drawUSBQuery();
        gpio_wakeup_disable(USB_POWER_TRIGGER);
        gpio_wakeup_enable(USB_POWER_TRIGGER,GPIO_INTR_HIGH_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }

    if(wakeupReason==ESP_SLEEP_WAKEUP_TIMER) //if we have woken up from deep sleep because of a timer, this will trigger and shutdown the device
    {
        Device::dubbleBuzz();
        device.displayOffScreen("POWER OFF");
        renderer->deepSleep();
        Device::shutdown();
    }
    
    last_interaction_time = esp_timer_get_time();
    esp_sleep_enable_timer_wakeup(device.deviceSettings.standbyTimeout*60000000 + 100000); //start the sleep timer




    while(1) //this is the main interaction loop
    {
        if(renderer->epd.displaysPoweredOn)
        {
            esp_sleep_enable_timer_wakeup(displayTimeOut*1000000 + 100000); //if the displays are powered on, we will wakeup  in 10 seconds to turn them off
        }
        Device::getInstance().clearButtonLatches();
        Device::getInstance().setLatchTimeOut(10000);
        if(!gpio_get_level(GPIO_NUM_16)) //if usb is not connected we go into light sleep
        {
            esp_light_sleep_start();
        }
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);  
        esp_sleep_enable_timer_wakeup(device.deviceSettings.standbyTimeout*60000000 + 100000); //reset the timers

        vTaskDelay(pdMS_TO_TICKS(20)); //short delay for debouncing
        device.pollButtons();

        if (esp_timer_get_time() - renderer->epd.DisplayPowerTimer > displayTimeOut*1000000 && renderer->epd.displaysPoweredOn) //check if we wake up because the displays were still on
        {
            ESP_LOGI(TAG, "Powering down still active displays...");
            renderer->epd.SleepBoth(); //turn both displays off
            //esp_sleep_enable_timer_wakeup((device.deviceSettings.standbyTimeout-displayTimeOut)*60000000 + 100000); //and reset the sleep timer
            continue; //and go back to sleep
        }

        int64_t now = esp_timer_get_time();
        if(gpio_get_level(GPIO_NUM_42))
        {
            while(gpio_get_level(GPIO_NUM_42)) { //check if we wake because the power button is pressed
                vTaskDelay(10);
                if(esp_timer_get_time() - now> 3000000) //if the power button is pressed for 3 seconds, we enter the shutdown procedure
                {
                    if(device.reader->book && device.state==Device::State::Reading)
                    {
                        device.bookHandler->saveBook(device.reader->book);
                    }
                    Device::dubbleBuzz();
                    device.displayOffScreen("POWER OFF");
                    renderer->deepSleep();
                    Device::shutdown();
                }
            }
        vTaskDelay(pdMS_TO_TICKS(100));
        }
        



       
        if(gpio_get_level(GPIO_NUM_16) && !gpio_get_level(USB_POWER_TRIGGER) && device.usb_state==Device::usbState::Unconnected) //check if we wake because of a usb cable being plugged in
        {
            if(device.state!=Device::State::simpleReader)
            {
                device.usb_state=Device::usbState::Query;
                device.notificationHandler->drawUSBQuery();
                gpio_wakeup_disable(USB_POWER_TRIGGER);
                gpio_wakeup_enable(USB_POWER_TRIGGER,GPIO_INTR_HIGH_LEVEL);
                esp_sleep_enable_gpio_wakeup();
                continue;
            }
            else
            {
                device.usb_state=Device::usbState::Charging;
                gpio_wakeup_disable(USB_POWER_TRIGGER);
                gpio_wakeup_enable(USB_POWER_TRIGGER,GPIO_INTR_HIGH_LEVEL);
                esp_sleep_enable_gpio_wakeup();
                continue;
            }
        }

        if(!gpio_get_level(GPIO_NUM_16) && device.usb_state!=Device::usbState::Unconnected) //check if the usb cable is now disconnected
        {
            device.usb_state=Device::usbState::Unconnected;
            gpio_wakeup_disable(USB_POWER_TRIGGER);
            gpio_wakeup_enable(USB_POWER_TRIGGER,GPIO_INTR_LOW_LEVEL);
            esp_sleep_enable_gpio_wakeup();
            if(device.state==Device::State::Reading)
            {
                renderer->drawBattery(reader->rightPageFrameBuffer,Device::getInstance().getBatteryPercentage());
                renderer->epd.DisplayPictureBoth(reader->leftPageFrameBuffer,reader->rightPageFrameBuffer);
            }
            else if(device.state!=Device::State::simpleReader)
            {
                menuHandler->drawMenu();
            }
        }

        //check if we wake because we need to enter deep sleep
        now = esp_timer_get_time();
        if (now - last_interaction_time > device.deviceSettings.standbyTimeout*60000000) {
            ESP_LOGI(TAG, "Entering deep sleep due to inactivity...");
            if(device.reader->book && device.state==Device::State::Reading)
            {
                device.bookHandler->saveBook(device.reader->book);
            }

            device.displayOffScreen("STANDBY");
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); //and now we enter deep sleep
            configure_buttons_deep_sleep();
            if(device.deviceSettings.standbyShutdown!=0)
            {
                esp_sleep_enable_timer_wakeup((uint64_t)device.deviceSettings.standbyShutdown *    24ULL * 60ULL * 60ULL * 1000000ULL);
                //esp_sleep_enable_timer_wakeup(120 * 1000000ULL);
            }
            renderer->deepSleep();
            esp_deep_sleep_start();
        }
        last_interaction_time = now;

        //Now, we check if the buttons are pressed, and act depending on the current state:
        if(device.buttonLatchedStates[PAGE_LEFT_BUTTON] || device.buttonStates[PAGE_LEFT_BUTTON] )
        {
            if(device.usb_state==Device::usbState::Query)
            {
                Device::buzz();
                device.usb_state=Device::usbState::Charging;
                device.notificationHandler->drawUSBQuery();
                if(device.state==Device::State::Reading) reader->openPage();
                else menuHandler->drawMenu();
                Device::buzz();
                continue;
            }
            else if(device.state==Device::State::Menu)
            {
                menuHandler->leftPageAction();
            }
            else if(device.state==Device::State::Reading)
            {
                reader->leftPageAction();
            }
            else if(device.state==Device::State::simpleReader)
            {
                simpleReader->leftPageAction();
            }
        }
        if(device.buttonLatchedStates[MIDDLE_BUTTON] || device.buttonStates[MIDDLE_BUTTON])
        {
            if(device.usb_state==Device::usbState::FileTransfer || device.usb_state==Device::usbState::Query)
            {

            }
            else if(device.state==Device::State::Menu)
            {
                menuHandler->middleButtonAction();
            }
            else if(device.state==Device::State::Reading)
            {
                reader->middleButtonAction();
            }
        }
        if(device.buttonLatchedStates[ARROW_UP_BUTTON] || device.buttonStates[ARROW_UP_BUTTON])
        {
            if(device.usb_state==Device::usbState::Query)
            {
                continue;
            }
            else if(device.state==Device::State::Menu)
            {
                menuHandler->upButtonAction();
            }
            else if(device.state==Device::State::Reading)
            {
                reader->upButtonAction();
            }
        }
        if(device.buttonLatchedStates[ARROW_DOWN_BUTTON] || device.buttonStates[ARROW_DOWN_BUTTON])
        {   
            ESP_LOGI("MAIN", "Main loop button press");
            if(device.usb_state==Device::usbState::Query)
            {
                continue;
            }
            else if(device.state==Device::State::Menu)
            {
                menuHandler->downButtonAction();
            }
            else if(device.state==Device::State::Reading)
            {
                reader->downButtonAction();
            }
        }
        if(device.buttonLatchedStates[PAGE_RIGHT_BUTTON] || device.buttonStates[PAGE_RIGHT_BUTTON])
        {
            if(device.usb_state==Device::usbState::Query)
            {
                Device::buzz();
                device.usb_state=Device::usbState::FileTransfer;
                device.notificationHandler->drawUSBQuery();
                device.state=Device::State::USB_FileTransfer;
                Device::buzz();
            }
            else if(device.state==Device::State::Menu)
            {
                menuHandler->rightPageAction();
            }
            else if(device.state==Device::State::Reading)
            {
                reader->rightPageAction();
            }
            else if(device.state==Device::State::simpleReader)
            {
                simpleReader->rightPageAction();
            }
        }
    //check if we have entered file transfer mode
    if(device.state == Device::State::USB_FileTransfer) {
        if(device.reader->book)
        {
            device.bookHandler->saveBook(device.reader->book);
        }

        //stop usb cdc and unmount sd card
        tinyusb_console_deinit(TINYUSB_CDC_ACM_0);
        tinyusb_cdcacm_deinit(TINYUSB_CDC_ACM_0);
        tinyusb_driver_uninstall();
        delete(sd);
        vTaskDelay(10);

        ESP_LOGI(TAG, "USB Mass Storage mode");
        if (usb_msc_sdmmc_start(GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39, 1) == ESP_OK) {

            // Wait until USB cable disconnected (GPIO low)
            while(gpio_get_level(GPIO_NUM_16)) {
                vTaskDelay(10);
            }

            ESP_LOGI(TAG, "USB disconnected, stopping MSC...");
            usb_msc_stop();  // stop MSC

            
        } else {
            ESP_LOGE(TAG, "USB MSC failed to start");
        }

        vTaskDelay(10);
        //remount the sd card
        sd = new SDCard("/sdcard", GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39);

        //restart usb cdc:
        tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
        static const char* s_string_desc[] = {
        (const char[]){0x09, 0x04}, // 0: English (0x0409)
        "Diptyx",                // 1: Manufacturer
        "Diptyx E-reader",         // 2: Product
        "123456",                   // 3: Serial
        "SD Card",                  
        };
    tusb_cfg.descriptor.string              = s_string_desc;
    tusb_cfg.descriptor.string_count        = sizeof(s_string_desc) / sizeof(s_string_desc[0]);
        ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
        tinyusb_config_cdcacm_t acm_cfg = {
            .cdc_port = TINYUSB_CDC_ACM_0,
        };
        ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
        ESP_ERROR_CHECK(tinyusb_console_init(TINYUSB_CDC_ACM_0));

        esp_restart(); //restart the device to init everything properly
    }


    }

    vTaskDelete(NULL);  
}

void buttonPoller(void *param) //the buttonpoller runs on a different core from the main task, it reads buttons async
{
    while(true)
    {
        Device::getInstance().pollButtons();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}


extern "C" void app_main(void)
{
    //startup procedure:
    //configure the power pin, rumble, and status led
    //power pin
    gpio_reset_pin(GPIO_NUM_38);
    gpio_set_direction(GPIO_NUM_38, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_38, 1);
    gpio_hold_en(GPIO_NUM_38);
    //gpio_deep_sleep_hold_en();

    //led pin
    gpio_reset_pin(GPIO_NUM_48);
    gpio_set_direction(GPIO_NUM_48, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_48, 1);

    //battery sense control
    gpio_reset_pin(B_SENSE_CTRL);
    gpio_set_direction(B_SENSE_CTRL, GPIO_MODE_OUTPUT);
    gpio_set_level(B_SENSE_CTRL, 0);
    gpio_hold_en(B_SENSE_CTRL);
    gpio_deep_sleep_hold_en();
    
    //rumble pin
    gpio_reset_pin(GPIO_NUM_47);
    gpio_set_direction(GPIO_NUM_47, GPIO_MODE_OUTPUT);
    //short rumble:
    gpio_set_level(GPIO_NUM_47, 1);
    vTaskDelay(4);
    gpio_set_level(GPIO_NUM_47, 0);
    

    //init the usb cdc serial
    ESP_LOGI(TAG, "USB initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.phy.self_powered = true;
    tusb_cfg.phy.vbus_monitor_io = GPIO_NUM_16;
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
    ESP_ERROR_CHECK(tinyusb_console_init(TINYUSB_CDC_ACM_0));
    ESP_LOGI(TAG, "USB initialization DONE");
    ESP_LOGI(TAG, "booting");
    ESP_ERROR_CHECK(nvs_flash_init());


    //start the app tasks
    xTaskCreatePinnedToCore(main_task, "main_task", 70000, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(buttonPoller, "buttonPoller", 5000, NULL, 5, NULL, 0);
}