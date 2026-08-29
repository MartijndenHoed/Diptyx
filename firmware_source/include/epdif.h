#ifndef EPDIF_H
#define EPDIF_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// GPIO Pin assignments (update as needed)
#define RST_PIN_LEFT     GPIO_NUM_8
#define DC_PIN_LEFT      GPIO_NUM_9
#define CS_PIN_LEFT      GPIO_NUM_10
#define BUSY_PIN_LEFT    GPIO_NUM_7

#define RST_PIN_RIGHT     GPIO_NUM_17
#define DC_PIN_RIGHT      GPIO_NUM_18
#define CS_PIN_RIGHT      GPIO_NUM_21
#define BUSY_PIN_RIGHT    GPIO_NUM_14

#ifdef __cplusplus
}
#endif

class EpdIf {
public:
    EpdIf(void);
    ~EpdIf(void);

    static int  initSPI(void);
    static void shutDownSPI(void);
    static void DigitalWrite(gpio_num_t pin, int value); 
    static int  DigitalRead(gpio_num_t pin);
    static void DelayMs(unsigned int delaytime);
    static void SpiTransfer(uint8_t data,gpio_num_t cs_pin);
    static void SpiTransfer(const unsigned char *data,gpio_num_t cs_pin,int length);
    static spi_device_handle_t spi_handle;
    static bool SPIactive;

private:
    
};

#endif // EPDIF_H