#include <stdlib.h>
#include "epd5in83b_V2.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "device.h"

static const char *TAG = "EPD";

#define T1 20
#define T2 1
#define T3 0
#define T4 0

static const unsigned char lut_20_LUTC_partial[] = { 0x00, 0x14, 0x01, 0x00, 0x00, 0x01 };
static const unsigned char lut_21_LUTWW_partial[] = { 0x00, 0x14, 0x01, 0x00, 0x00, 0x01 };
static const unsigned char lut_22_LUTKW_partial[] = { 0x80, 0x14, 0x01, 0x00, 0x00, 0x01 };
static const unsigned char lut_23_LUTWK_partial[] = { 0x40, 0x14, 0x01, 0x00, 0x00, 0x01 };
static const unsigned char lut_24_LUTKK_partial[] = { 0x00, 0x14, 0x01, 0x00, 0x00, 0x01 };

static const unsigned char lut_20_LUTC_full[] = {0x00,	0x1E,	0x1E, 	0x1E,	0x01,	0x01};
static const unsigned char lut_21_LUTWW_full[] = {0x60,	0x1E,	0x1E,	0x1E,	0x01,	0x01};
static const unsigned char lut_22_LUTKW_full[] = {0x60,	0x1E,	0x1E,	0x1E,	0x01,	0x01};
static const unsigned char lut_23_LUTWK_full[] = {0x64,	0x1E,	0x1E,	0x1E,	0x01,	0x01};
static const unsigned char lut_24_LUTKK_full[] = {0x24,	0x1E,	0x1E,	0x1E,	0x01,	0x01};


Epd::~Epd() {}

Epd::Epd() {
    reset_pin_left = RST_PIN_LEFT;
    dc_pin_left  = DC_PIN_LEFT;
    cs_pin_left  = CS_PIN_LEFT;
    busy_pin_left  = BUSY_PIN_LEFT;

    reset_pin_right = RST_PIN_RIGHT;
    dc_pin_right  = DC_PIN_RIGHT;
    cs_pin_right  = CS_PIN_RIGHT;
    busy_pin_right  = BUSY_PIN_RIGHT;

    width = EPD_WIDTH / 8;
    height = EPD_HEIGHT;
    tmpBuf = (uint8_t *)heap_caps_malloc(width * height, MALLOC_CAP_DMA);
}

void Epd::activateDisplay(bool isLeft)
{
    if(isLeft)
    {
        reset_pin = reset_pin_left;
        dc_pin  = dc_pin_left;
        cs_pin  = cs_pin_left;
        busy_pin  = busy_pin_left; 
    }
    else
    {
        reset_pin = reset_pin_right;
        dc_pin  = dc_pin_right;
        cs_pin  = cs_pin_right;
        busy_pin  = busy_pin_right; 
    }
}

int Epd::Init(void) {
    if (initSPI() != 0) {
        return -1;
    }
    InitNormalBoth();
    displaysPoweredOn = true;
    DisplayPowerTimer = esp_timer_get_time();
    return 0;
}

void Epd::SendCommand(unsigned char command) {
    DigitalWrite(dc_pin, 0);
    SpiTransfer(command,cs_pin);
}

void Epd::SendData(unsigned char data) {
    DigitalWrite(dc_pin, 1);
    SpiTransfer(data,cs_pin);
}

void Epd::WaitUntilIdle(void) {
    ESP_LOGI(TAG, "e-Paper busy...");
    int idleTimer = 0;
    while (1) {
        //SendCommand(0x71);
        if (DigitalRead(busy_pin) == 1) break;
        DelayMs(20);
        idleTimer += 20;
        if(idleTimer>100000) //if we stay in idle for more than 10 seconds, something has gone wrong and we reset
        {
            Device::getInstance().errorBlinkLED(4);
            esp_restart();
        }
    }
    ESP_LOGI(TAG, "e-Paper ready.");
}

void Epd::Reset(void) {
    DigitalWrite(reset_pin, 0);
    DelayMs(10);
    DigitalWrite(reset_pin, 1);
    DelayMs(10);
}

UBYTE reverseByte(UBYTE b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}


void Epd::Clear(bool isLeft) {
    initSPI();
    DelayMs(10);
    InitNormal(isLeft);
    activateDisplay(isLeft);
    SendCommand(0x13);  // Write new image data (B/W)
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            SendData(0x00);  // All white (each bit = 1)
        }
    }

    SendCommand(0x12);  // Trigger display refresh
    WaitUntilIdle();
}

void Epd::Sleep(bool isLeft) {
    activateDisplay(isLeft);
    SendCommand(0x02); // POWER_OFF
    WaitUntilIdle();
    displaysPoweredOn = false;
    shutDownSPI();
}

void Epd::SleepBoth() {
    ESP_LOGI("EPD", "Shutting down displays...");
    activateDisplay(true);
    SendCommand(0x02); // POWER_OFF
    activateDisplay(false);
    SendCommand(0x02); // POWER_OFF
    WaitUntilIdleBoth();
    displaysPoweredOn = false;
    shutDownSPI();
}



void Epd::DeepSleep(bool isLeft) {
    initSPI();
    DelayMs(10);
    activateDisplay(isLeft);
    SendCommand(0x02); // POWER_OFF
    WaitUntilIdle();
    SendCommand(0x07); // DEEP_SLEEP
    SendData(0xA5);
    shutDownSPI();
}



void Epd::SetPartialLUT(void) {
    // Send partial update LUTs
    SendCommand(0x20);
    for (int i = 0; i < sizeof(lut_20_LUTC_partial); ++i) SendData(lut_20_LUTC_partial[i]);
    for (int i = 0; i < 42 - sizeof(lut_20_LUTC_partial); ++i) SendData(0x00);

    SendCommand(0x21);
    for (int i = 0; i < sizeof(lut_21_LUTWW_partial); ++i) SendData(lut_21_LUTWW_partial[i]);
    for (int i = 0; i < 42 - sizeof(lut_21_LUTWW_partial); ++i) SendData(0x00);

    SendCommand(0x22);
    for (int i = 0; i < sizeof(lut_22_LUTKW_partial); ++i) SendData(lut_22_LUTKW_partial[i]);
    for (int i = 0; i < 42 - sizeof(lut_22_LUTKW_partial); ++i) SendData(0x00);

    SendCommand(0x23);
    for (int i = 0; i < sizeof(lut_23_LUTWK_partial); ++i) SendData(lut_23_LUTWK_partial[i]);
    for (int i = 0; i < 42 - sizeof(lut_23_LUTWK_partial); ++i) SendData(0x00);

    SendCommand(0x24);
    for (int i = 0; i < sizeof(lut_24_LUTKK_partial); ++i) SendData(lut_24_LUTKK_partial[i]);
    for (int i = 0; i < 42 - sizeof(lut_24_LUTKK_partial); ++i) SendData(0x00);
}

void Epd::SetFullLUT(void) {
    // Send full update LUTs
    SendCommand(0x20);
    for (int i = 0; i < sizeof(lut_20_LUTC_partial); ++i) SendData(lut_20_LUTC_full[i]);
    for (int i = 0; i < 42 - sizeof(lut_20_LUTC_partial); ++i) SendData(0x00);

    SendCommand(0x21);
    for (int i = 0; i < sizeof(lut_21_LUTWW_partial); ++i) SendData(lut_21_LUTWW_full[i]);
    for (int i = 0; i < 42 - sizeof(lut_21_LUTWW_partial); ++i) SendData(0x00);

    SendCommand(0x22);
    for (int i = 0; i < sizeof(lut_22_LUTKW_partial); ++i) SendData(lut_22_LUTKW_full[i]);
    for (int i = 0; i < 42 - sizeof(lut_22_LUTKW_partial); ++i) SendData(0x00);

    SendCommand(0x23);
    for (int i = 0; i < sizeof(lut_23_LUTWK_partial); ++i) SendData(lut_23_LUTWK_full[i]);
    for (int i = 0; i < 42 - sizeof(lut_23_LUTWK_partial); ++i) SendData(0x00);

    SendCommand(0x24);
    for (int i = 0; i < sizeof(lut_24_LUTKK_partial); ++i) SendData(lut_24_LUTKK_full[i]);
    for (int i = 0; i < 42 - sizeof(lut_24_LUTKK_partial); ++i) SendData(0x00);
}

void Epd::InitPartial(bool isLeft) {
    activateDisplay(isLeft);
    Reset();

    initPhase1();
    DelayMs(10);
    WaitUntilIdle();
    initPhase2();  
    SetPartialLUT(); 

    //SetFullLUT();
    displaysPoweredOn = true;
    DisplayPowerTimer = esp_timer_get_time();
}

void Epd::InitPartialBoth() {
    activateDisplay(true);
    
    Reset();
    initPhase1();
   

    activateDisplay(false);
    Reset();
    initPhase1();


    WaitUntilIdleBoth();

    activateDisplay(true);
    initPhase2();
    SetPartialLUT();
    activateDisplay(false);
    initPhase2();
    SetPartialLUT();
    displaysPoweredOn = true;
    DisplayPowerTimer = esp_timer_get_time();
}

void Epd::InitNormal(bool isLeft) {
    activateDisplay(isLeft);
    Reset();

    initPhase1();
    DelayMs(10);
    WaitUntilIdle();
    initPhase2();  
    SetFullLUT(); 
    displaysPoweredOn = true;
    DisplayPowerTimer = esp_timer_get_time();
}

void Epd::initPhase1()
{
   SendCommand(0x01); // POWER SETTING
   SendData(0x03);
   SendData(0x17); // VGH=20V,VGL=-20V
   SendData(0x3f); // VDH=15V
   SendData(0x3f); // VDL=-15V
    SendData(0x03);
	

    if(reset_pin == reset_pin_left)
    {
        SendCommand(0x82); // vcom_DC setting
        SendData(Device::getInstance().deviceSettings.vcomLeft);
    }
    else
    {    
        SendCommand(0x82); // vcom_DC setting
        SendData(Device::getInstance().deviceSettings.vcomRight);
    }

    SendCommand(0x06);
    SendData(0x17);
    SendData(0x17);
    SendData(0x3D);
    SendData(0x3C);

    SendCommand(0x30); //changed, added 
    SendData(0x07);//CONSIDER CHANGING

    SendCommand(0x52); //changed, added 
    SendData(0x02);//CONSIDER CHANGING

    SendCommand(0xE3); //changed, added 
    SendData(0x88);//CONSIDER CHANGING

    SendCommand(0x41); //set temperature offset so it doesn't slow down to a crawl when it's a bit cold
    SendData(0x07);

    SendCommand(0x04); // POWER ON
    DelayMs(10);
}

void Epd::initPhase2()
{
    SendCommand(0x00); // PANEL SETTING
    SendData(0x3F);
    SendData(0x09);

    SendCommand(0x61); // TRES
    SendData(0x02);    // source 648
    SendData(0x88);
    SendData(0x01);    // gate 480
    SendData(0xE0);

    SendCommand(0x15);
    SendData(0x00);

    SendCommand(0x50); // VCOM AND DATA INTERVAL SETTING
    if(Device::getInstance().deviceSettings.nightMode) SendData(0x38);
    else SendData(0x18);
    SendData(0x07);

    SendCommand(0x60); // TCON SETTING
    SendData(0x22);
}

void Epd::InitNormalBoth() {
    activateDisplay(true);
    
    Reset();
    initPhase1();
   

    activateDisplay(false);
    Reset();
    initPhase1();


    WaitUntilIdleBoth();

    activateDisplay(true);
    initPhase2();
    SetFullLUT();
    activateDisplay(false);
    initPhase2();
    SetFullLUT();

    displaysPoweredOn = true;
    DisplayPowerTimer = esp_timer_get_time();
}

void Epd::DisplayPicture(bool isLeft, const UBYTE *blackimage) {
    initSPI();
    DelayMs(10);

    activateDisplay(isLeft);
    bool isFlipped = isLeft;

    if(partialUpdatesRemaining[isLeft]==0)
    {
        InitNormal(isLeft);
        partialUpdatesRemaining[isLeft] = 5;
    }
    else
    {
        InitPartial(isLeft);
        partialUpdatesRemaining[isLeft]--;
    }

    SendCommand(0x13);  // New image data for B/W panel
    sendScreenBuffer(blackimage,isFlipped);
    SendCommand(0x12);  // Display refresh
    WaitUntilIdle();
    Sleep(isLeft);
}

void Epd::sendScreenBuffer(const unsigned char *screenBuffer, bool isFlipped)
{
    size_t bufSize = width * height; // total bytes


    if(isFlipped)
    {    
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                tmpBuf[i + j * width] = reverseByte(
                    screenBuffer[(width - 1 - i) + (height - 1 - j) * width]
                );
            }
        }
    }
    else
    {
        memcpy(tmpBuf,screenBuffer,bufSize);
    }

    if(Device::getInstance().deviceSettings.nightMode)
    {
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                tmpBuf[i + j * width] = ~tmpBuf[i + j * width];
            }
        }
    }

    // Send the flipped buffer in one transaction
    DigitalWrite(dc_pin, 1);
    SpiTransfer(tmpBuf,cs_pin,bufSize*4); 
    SpiTransfer(tmpBuf+bufSize/2,cs_pin,bufSize*4);
}

void Epd::forceRefresh(void)
{
    this->partialUpdatesRemaining[0] = 0;
    this->partialUpdatesRemaining[1] = 0;
}

void Epd::forceDoubleRefresh()
{
    this->forcedDoubleRefresh = true;
    this->partialUpdatesRemaining[0] = 0;
    this->partialUpdatesRemaining[1] = 0;
}

void Epd::DisplayPictureBoth(const unsigned char *leftScreenBuffer, const unsigned char *rightScreenBuffer)
{
    initSPI();
    DelayMs(10);

    DisplayPowerTimer = esp_timer_get_time();
    if(Device::getInstance().state!=Device::State::Menu && Device::getInstance().reader->state!=Reader::State::BookMarkMenu) quickMode = false;
    if(partialUpdatesRemaining[true]==0 || (Device::getInstance().state!=Device::State::Reading && Device::getInstance().deviceSettings.sunlightMode==true &&  Device::getInstance().deviceSettings.sunlightFullRefresh==true))
    {
        InitNormalBoth();
        if(!forcedDoubleRefresh) partialUpdatesRemaining[true] = Device::getInstance().deviceSettings.displayRefresh;
        forcedDoubleRefresh = false;
        quickMode = false;
    }
    else if(quickMode==false || displaysPoweredOn == false)
    {
        InitPartialBoth();
        partialUpdatesRemaining[true]--;
        if(Device::getInstance().state==Device::State::Menu || Device::getInstance().reader->state==Reader::State::BookMarkMenu) quickMode = true;
    }
    else if(quickMode==true)
    {
        partialUpdatesRemaining[true]--;
        if(Device::getInstance().state==Device::State::Menu || Device::getInstance().reader->state==Reader::State::BookMarkMenu) quickMode = true;
    }
    
    activateDisplay(true);
    SendCommand(0x13);  // New image data for B/W panel
    sendScreenBuffer(leftScreenBuffer,true);
    SendCommand(0x12);  // Display refresh

    activateDisplay(false);
    SendCommand(0x13);  // New image data for B/W panel
    sendScreenBuffer(rightScreenBuffer,false);
    SendCommand(0x12);  // Display refresh
    WaitUntilIdleBoth();

    if( (Device::getInstance().state!=Device::State::Menu && Device::getInstance().reader->state!=Reader::State::BookMarkMenu) || Device::getInstance().deviceSettings.sunlightMode) SleepBoth();
}




void Epd::DisplayPictureBoth(const unsigned char *leftScreenBuffer, const unsigned char *rightScreenBuffer,std::function<void()> callback )
{
    initSPI();
    DelayMs(10);

    if(partialUpdatesRemaining[true]==0  || (Device::getInstance().state!=Device::State::Reading && Device::getInstance().deviceSettings.sunlightMode==true && Device::getInstance().deviceSettings.sunlightFullRefresh==true))
    {
        InitNormalBoth();
        if(!forcedDoubleRefresh) partialUpdatesRemaining[true] = Device::getInstance().deviceSettings.displayRefresh;
        forcedDoubleRefresh = false;
    }
    else
    {
        InitPartialBoth();
        partialUpdatesRemaining[true]--;
    }
    activateDisplay(true);
    SendCommand(0x13);  // New image data for B/W panel
    sendScreenBuffer(leftScreenBuffer,true);
    SendCommand(0x12);  // Display refresh

    activateDisplay(false);
    SendCommand(0x13);  // New image data for B/W panel
    sendScreenBuffer(rightScreenBuffer,false);
    SendCommand(0x12);  // Display refresh
    WaitUntilIdleBoth(callback);
    SleepBoth();
}




void Epd::WaitUntilIdleBoth(std::function<void()> callback)
{
    callback();
    int idleTimer = 0;
    while (1) {
        //SendCommand(0x71);
        if (DigitalRead(BUSY_PIN_LEFT) == 1 && DigitalRead(BUSY_PIN_RIGHT) == 1) break;
        DelayMs(20);
        idleTimer += 20;
        if(idleTimer>100000) //if we stay in idle for more than 10 seconds, something has gone wrong and we reset
        {
            Device::getInstance().errorBlinkLED(3);
            esp_restart();
        }
    }
    DelayMs(10);
    ESP_LOGI(TAG, "e-Papers ready.");
}



void Epd::WaitUntilIdleBoth(void)
{
    ESP_LOGI(TAG, "e-Papers busy...");
    int idleTimer = 0;
    while (1) {
        //SendCommand(0x71);
        if (DigitalRead(BUSY_PIN_LEFT) == 1 && DigitalRead(BUSY_PIN_RIGHT) == 1) break;
        DelayMs(20);
        idleTimer += 20;
        if(idleTimer>100000) //if we stay in idle for more than 10 seconds, something has gone wrong and we reset
        {
            Device::getInstance().errorBlinkLED(3);
            esp_restart();
        }
    }
    DelayMs(20);
    ESP_LOGI(TAG, "e-Papers ready.");
}


void Epd::DisplayPicturePartial(bool isLeft, const UBYTE *blackimage,
                                int partialMinX, int partialMinY,
                                int partialMaxX, int partialMaxY) {
    initSPI();
    DelayMs(10);

        DisplayPowerTimer = esp_timer_get_time();
         ESP_LOGI("display", "partial updates remaining: %d",partialUpdatesRemaining[true]);
    if(Device::getInstance().state!=Device::State::Menu && Device::getInstance().reader->state!=Reader::State::BookMarkMenu) quickMode = false;
    if(partialUpdatesRemaining[true]==0)
    {
        InitNormalBoth();
        if(!forcedDoubleRefresh) partialUpdatesRemaining[true] = Device::getInstance().deviceSettings.displayRefresh;
        forcedDoubleRefresh = false;
        quickMode = false;
    }
    else if(quickMode==false || displaysPoweredOn == false)
    {
        InitPartialBoth();
        partialUpdatesRemaining[true]--;
        if(Device::getInstance().state==Device::State::Menu || Device::getInstance().reader->state==Reader::State::BookMarkMenu) quickMode = true;
    }
    else if(quickMode==true)
    {
        partialUpdatesRemaining[true]--;
        if(Device::getInstance().state==Device::State::Menu || Device::getInstance().reader->state==Reader::State::BookMarkMenu) quickMode = true;
    }
    
    
    activateDisplay(isLeft);
    const bool isFlipped = isLeft;      // same rule as DisplayPicture
    const int pixelWidth  = width * 8;  // width = bytes/line
    const int pixelHeight = height;

    // ---- transform coordinates for 180° flip ----
    int partialMinY_new = EPD_HEIGHT - partialMaxX;
    int partialMaxY_new = EPD_HEIGHT - partialMinX;
    partialMinX = EPD_WIDTH - partialMaxY;
    partialMaxX = EPD_WIDTH - partialMinY;
    partialMinY = partialMinY_new;
    partialMaxY = partialMaxY_new;

    // ---- clamp to valid bounds ----
    if (partialMinX < 0) partialMinX = 0;
    if (partialMinY < 0) partialMinY = 0;
    if (partialMaxX >= pixelWidth)  partialMaxX = pixelWidth  - 1;
    if (partialMaxY >= pixelHeight) partialMaxY = pixelHeight - 1;
    if (partialMinX > partialMaxX || partialMinY > partialMaxY) return;

    // ---- align X to byte boundaries ----
    int x0 = partialMinX & ~7;
    int x1 = partialMaxX | 7;
    if (x1 >= pixelWidth) x1 = pixelWidth - 1;

    // ---- map to physical coords ----
    int physX0, physX1, physY0, physY1;
    if (!isFlipped) {
        physX0 = x0;                         physX1 = x1;
        physY0 = partialMinY;                physY1 = partialMaxY;
    } else {
        physX0 = (pixelWidth  - 1) - x1;     physX1 = (pixelWidth  - 1) - x0;
        physY0 = (pixelHeight - 1) - partialMaxY;
        physY1 = (pixelHeight - 1) - partialMinY;
    }

    // ---- enter partial mode & set window ----
    SendCommand(0x91); // PARTIAL IN
    SendCommand(0x90); // PARTIAL WINDOW
    SendData(physX0 >> 8); SendData(physX0 & 0xFF);
    SendData(physX1 >> 8); SendData(physX1 & 0xFF);
    SendData(physY0 >> 8); SendData(physY0 & 0xFF);
    SendData(physY1 >> 8); SendData(physY1 & 0xFF);
    SendData(0x01);

    // ---- prepare sub-buffer ----
    const int nBytesPerLine = (physX1 - physX0 + 1) / 8;
    const int nLines        = (physY1 - physY0 + 1);
    const size_t bufSize    = nBytesPerLine * nLines;

    uint8_t *tmpBuf = (uint8_t *)heap_caps_malloc(bufSize, MALLOC_CAP_DMA);
    if (!tmpBuf) {
        ESP_LOGE("epd", "Out of memory in DisplayPicturePartialFast");
        SendCommand(0x92); // PARTIAL OUT
        return;
    }

    // ---- copy + flip into tmpBuf ----
    for (int line = 0; line < nLines; ++line) {
        int yPhys = physY0 + line;
        if (!isFlipped) {
            int ySrc = yPhys;
            int xByteSrc0 = x0 / 8;
            memcpy(&tmpBuf[line * nBytesPerLine],
                   &blackimage[ySrc * width + xByteSrc0],
                   nBytesPerLine);
        } else {
            int ySrc = (pixelHeight - 1) - yPhys;
            int xByteSrcStart = x1 / 8;
            for (int k = 0; k < nBytesPerLine; ++k) {
                UBYTE b = blackimage[ySrc * width + (xByteSrcStart - k)];
                tmpBuf[line * nBytesPerLine + k] = reverseByte(b);
            }
        }
    }

    // ---- send sub-buffer in one go ----
    SendCommand(0x13); // write B/W RAM
    DigitalWrite(dc_pin, 1);
    SpiTransfer(tmpBuf, cs_pin, bufSize * 8); // length in bits
    free(tmpBuf);

    // ---- refresh ----
    SendCommand(0x12); // REFRESH
    ESP_LOGI(TAG, "going into idle at time %d %d",1,esp_timer_get_time());
    WaitUntilIdle();
    ESP_LOGI(TAG, "leaving idle at time %d %d",1,esp_timer_get_time());
    SendCommand(0x92); // PARTIAL OUT
    if(Device::getInstance().state!=Device::State::Menu && Device::getInstance().reader->state!=Reader::State::BookMarkMenu) SleepBoth();
}

void Epd::DisplayPicturePartialFast(bool isLeft, const UBYTE *blackimage,
                                    int partialMinX, int partialMinY,
                                    int partialMaxX, int partialMaxY) {
    initSPI();
    DelayMs(10);

    activateDisplay(isLeft);

    const bool isFlipped = isLeft;      // same rule as DisplayPicture
    const int pixelWidth  = width * 8;  // width = bytes/line
    const int pixelHeight = height;

    // ---- transform coordinates for 180° flip ----
    int partialMinY_new = EPD_HEIGHT - partialMaxX;
    int partialMaxY_new = EPD_HEIGHT - partialMinX;
    partialMinX = EPD_WIDTH - partialMaxY;
    partialMaxX = EPD_WIDTH - partialMinY;
    partialMinY = partialMinY_new;
    partialMaxY = partialMaxY_new;

    // ---- clamp to valid bounds ----
    if (partialMinX < 0) partialMinX = 0;
    if (partialMinY < 0) partialMinY = 0;
    if (partialMaxX >= pixelWidth)  partialMaxX = pixelWidth  - 1;
    if (partialMaxY >= pixelHeight) partialMaxY = pixelHeight - 1;
    if (partialMinX > partialMaxX || partialMinY > partialMaxY) return;

    // ---- align X to byte boundaries ----
    int x0 = partialMinX & ~7;
    int x1 = partialMaxX | 7;
    if (x1 >= pixelWidth) x1 = pixelWidth - 1;

    // ---- map to physical coords ----
    int physX0, physX1, physY0, physY1;
    if (!isFlipped) {
        physX0 = x0;                         physX1 = x1;
        physY0 = partialMinY;                physY1 = partialMaxY;
    } else {
        physX0 = (pixelWidth  - 1) - x1;     physX1 = (pixelWidth  - 1) - x0;
        physY0 = (pixelHeight - 1) - partialMaxY;
        physY1 = (pixelHeight - 1) - partialMinY;
    }

    // ---- enter partial mode & set window ----
    SendCommand(0x91); // PARTIAL IN
    SendCommand(0x90); // PARTIAL WINDOW
    SendData(physX0 >> 8); SendData(physX0 & 0xFF);
    SendData(physX1 >> 8); SendData(physX1 & 0xFF);
    SendData(physY0 >> 8); SendData(physY0 & 0xFF);
    SendData(physY1 >> 8); SendData(physY1 & 0xFF);
    SendData(0x01);

    // ---- prepare sub-buffer ----
    const int nBytesPerLine = (physX1 - physX0 + 1) / 8;
    const int nLines        = (physY1 - physY0 + 1);
    const size_t bufSize    = nBytesPerLine * nLines;

    uint8_t *tmpBuf = (uint8_t *)heap_caps_malloc(bufSize, MALLOC_CAP_DMA);
    if (!tmpBuf) {
        ESP_LOGE("epd", "Out of memory in DisplayPicturePartialFast");
        SendCommand(0x92); // PARTIAL OUT
        return;
    }

    // ---- copy + flip into tmpBuf ----
    for (int line = 0; line < nLines; ++line) {
        int yPhys = physY0 + line;
        if (!isFlipped) {
            int ySrc = yPhys;
            int xByteSrc0 = x0 / 8;
            memcpy(&tmpBuf[line * nBytesPerLine],
                   &blackimage[ySrc * width + xByteSrc0],
                   nBytesPerLine);
        } else {
            int ySrc = (pixelHeight - 1) - yPhys;
            int xByteSrcStart = x1 / 8;
            for (int k = 0; k < nBytesPerLine; ++k) {
                UBYTE b = blackimage[ySrc * width + (xByteSrcStart - k)];
                tmpBuf[line * nBytesPerLine + k] = reverseByte(b);
            }
        }
    }

    // ---- send sub-buffer in one go ----
    SendCommand(0x13); // write B/W RAM
    DigitalWrite(dc_pin, 1);
    SpiTransfer(tmpBuf, cs_pin, bufSize * 8); // length in bits
    free(tmpBuf);

    // ---- refresh ----
    SendCommand(0x12); // REFRESH
    ESP_LOGI(TAG, "going into idle at time %d %d",1,esp_timer_get_time());
    WaitUntilIdle();
    ESP_LOGI(TAG, "leaving idle at time %d %d",1,esp_timer_get_time());
    SendCommand(0x92); // PARTIAL OUT
}
