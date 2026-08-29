#include "epdif.h"
#include "esp_log.h"

spi_device_handle_t EpdIf::spi_handle;
bool EpdIf::SPIactive = false;

EpdIf::EpdIf() {}

EpdIf::~EpdIf() {}

void EpdIf::DigitalWrite(gpio_num_t pin, int value) {
    gpio_set_level(pin, value);
}

int EpdIf::DigitalRead(gpio_num_t pin) {
    return gpio_get_level(pin);
}

void EpdIf::DelayMs(unsigned int delaytime) {
    vTaskDelay(delaytime / portTICK_PERIOD_MS);
}

void EpdIf::SpiTransfer(uint8_t data,gpio_num_t cs_pin) {
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    gpio_set_level(cs_pin, 0);  // Manually control CS
    spi_device_transmit(spi_handle, &t);
    gpio_set_level(cs_pin, 1);
}

void EpdIf::SpiTransfer(const unsigned char *data,gpio_num_t cs_pin,int length) {
    spi_transaction_t t = {};
    t.length = length;
    t.tx_buffer = data;
    gpio_set_level(cs_pin, 0);  // Manually control CS
    spi_device_transmit(spi_handle, &t);
    gpio_set_level(cs_pin, 1);
}


int EpdIf::initSPI(void) {
    if(!SPIactive)
    {
        esp_err_t ret;

        // Configure GPIOs
        gpio_reset_pin(RST_PIN_LEFT);
        gpio_reset_pin(DC_PIN_LEFT);
        gpio_reset_pin(CS_PIN_LEFT);
        gpio_reset_pin(BUSY_PIN_LEFT);

        gpio_set_direction(RST_PIN_LEFT, GPIO_MODE_OUTPUT);
        gpio_set_direction(DC_PIN_LEFT, GPIO_MODE_OUTPUT);
        gpio_set_direction(CS_PIN_LEFT, GPIO_MODE_OUTPUT);
        gpio_set_direction(BUSY_PIN_LEFT, GPIO_MODE_INPUT);

        gpio_set_drive_capability(RST_PIN_LEFT, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability(DC_PIN_LEFT, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability(CS_PIN_LEFT, GPIO_DRIVE_CAP_0);

        gpio_reset_pin(RST_PIN_RIGHT);
        gpio_reset_pin(DC_PIN_RIGHT);
        gpio_reset_pin(CS_PIN_RIGHT);
        gpio_reset_pin(BUSY_PIN_RIGHT);

        gpio_set_direction(RST_PIN_RIGHT, GPIO_MODE_OUTPUT);
        gpio_set_direction(DC_PIN_RIGHT, GPIO_MODE_OUTPUT);
        gpio_set_direction(CS_PIN_RIGHT, GPIO_MODE_OUTPUT);
        gpio_set_direction(BUSY_PIN_RIGHT, GPIO_MODE_INPUT);

        gpio_set_drive_capability(RST_PIN_RIGHT, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability(DC_PIN_RIGHT, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability(CS_PIN_RIGHT, GPIO_DRIVE_CAP_0);


        // SPI Configuration (VSPI: MOSI=GPIO11, SCLK=GPIO13)
        spi_bus_config_t buscfg = {
            .mosi_io_num = GPIO_NUM_12,
            .miso_io_num = -1,
            .sclk_io_num = GPIO_NUM_11,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 38880 //4096
        };

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 7 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 1;

        // Initialize the SPI bus
        ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) return -1;

        // Attach the device to the SPI bus
        ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
        if (ret != ESP_OK) return -1;

        gpio_set_drive_capability(GPIO_NUM_12, GPIO_DRIVE_CAP_0);
        gpio_set_drive_capability(GPIO_NUM_11, GPIO_DRIVE_CAP_0);
        SPIactive = true;
    }
    return 0;
}

void EpdIf::shutDownSPI()
{
    if(SPIactive)
    {
        if (spi_handle) {
            spi_bus_remove_device(spi_handle);
            spi_handle = nullptr;
        }

        // Free the SPI bus
        spi_bus_free(SPI2_HOST);

        gpio_reset_pin(GPIO_NUM_12); // MOSI
        gpio_reset_pin(GPIO_NUM_11); // SCLK

        gpio_reset_pin(RST_PIN_LEFT);
        gpio_reset_pin(DC_PIN_LEFT);
        gpio_reset_pin(CS_PIN_LEFT);

        gpio_reset_pin(RST_PIN_RIGHT);
        gpio_reset_pin(DC_PIN_RIGHT);
        gpio_reset_pin(CS_PIN_RIGHT);
        SPIactive = false;
    }
}