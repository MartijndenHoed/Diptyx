#pragma once

#include <hal/gpio_types.h>
#include <driver/sdmmc_types.h>
#include <driver/sdspi_host.h>

#include <string>

class SDCard
{
private:
  std::string m_mount_point;
  sdmmc_card_t *m_card;
  sdmmc_host_t m_host = SDSPI_HOST_DEFAULT();

public:
  SDCard(const char *mount_point,gpio_num_t clk, gpio_num_t cmd, gpio_num_t d0);
  ~SDCard();
  const std::string &get_mount_point() { return m_mount_point; }
  bool mountedSuccesfully = false;
};