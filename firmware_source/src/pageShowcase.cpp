#include "pageShowcase.h"
#include "esp_log.h"
#include <stdio.h>


static const char *TAG = "pageShowcase";

extern "C" {
extern const uint8_t binary_pageShowcase_html_start[] asm("_binary_pageShowcase_html_start");
extern const uint8_t binary_pageShowcase_html_end[] asm("_binary_pageShowcase_html_end");
}

const uint8_t* PageShowcase::start() {
    return binary_pageShowcase_html_start;
}

const uint8_t* PageShowcase::end() {
    return binary_pageShowcase_html_end;
}


size_t PageShowcase::size() {
    return end() - start();
}


const char* PageShowcase::getPage()
{
    return reinterpret_cast<const char*>(this->start());
}