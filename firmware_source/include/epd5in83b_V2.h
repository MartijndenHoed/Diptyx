#ifndef EPD5IN83B_V2_H
#define EPD5IN83B_V2_H

#include "epdif.h"
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

// Display resolution
#define EPD_WIDTH       648
#define EPD_HEIGHT      480

typedef unsigned int    UWORD;
typedef unsigned char   UBYTE;
typedef unsigned long   UDOUBLE;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class Epd : public EpdIf {
public:
    Epd();
    ~Epd();

    int  Init(void);
    void WaitUntilIdle(void);
    void WaitUntilIdleBoth(void);
    void WaitUntilIdleBoth(std::function<void()> callback);
    void Reset(void);
    void activateDisplay(bool isLeft);
    void DisplayPicture(bool isLeft, const UBYTE *blackimage);
    void DisplayPictureBoth(const unsigned char *leftScreenBuffer, const unsigned char *rightScreenBuffer);
    void DisplayPictureBoth(const unsigned char *leftScreenBuffer, const unsigned char *rightScreenBuffer,std::function<void()> callback);
    void sendScreenBuffer(const unsigned char *screenBuffer,bool isFlipped);
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void InitPartial(bool isLeft);
    void InitNormal(bool isLeft);

    void initPhase1(void);
    void initPhase2(void);

    void InitPartialBoth();
    void InitNormalBoth();
    void SetPartialLUT(void);
    void SetFullLUT(void);
    void Sleep(bool isLeft);
    void SleepBoth();
    void DeepSleep(bool isLeft);
    void Clear(bool isLeft);
    void forceRefresh(void);
    void DisplayPicturePartial(bool isLeft, const UBYTE *blackimage,int partialMinX,int partialMinY,int partialMaxX,int partialMaxY);
    void DisplayPicturePartialFast(bool isLeft, const UBYTE *blackimage,int partialMinX,int partialMinY,int partialMaxX,int partialMaxY);
    int partialUpdatesRemaining[2] = {0,0};
    void forceDoubleRefresh();
    bool forcedDoubleRefresh = false;
    bool quickMode = false;
    bool displaysPoweredOn = false;
    int64_t DisplayPowerTimer = 0;

private:
    uint8_t *tmpBuf;
    gpio_num_t reset_pin;
    gpio_num_t dc_pin;
    gpio_num_t cs_pin;
    gpio_num_t busy_pin;

    gpio_num_t reset_pin_left;
    gpio_num_t dc_pin_left;
    gpio_num_t cs_pin_left;
    gpio_num_t busy_pin_left;

    gpio_num_t reset_pin_right;
    gpio_num_t dc_pin_right;
    gpio_num_t cs_pin_right;
    gpio_num_t busy_pin_right;

    unsigned long width;
    unsigned long height;
};

#endif // __cplusplus

#endif /* EPD5IN83B_V2_H */