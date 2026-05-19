#include "can.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace can {

static const char *const TAG = "can.hub";

void CANHub::setup() {
  if (this->pin_ == nullptr) return;
  this->pin_->setup();
  gpio_num_t gpio_num = (gpio_num_t) this->pin_->get_pin();

  std::string name = App.get_name();
  int g_id = 0, p_id = 0;
  if (sscanf(name.c_str(), "peref%d-%d-%d", &g_id, &g_id, &p_id) == 3 || sscanf(name.c_str(), "perefn-%d-%d", &g_id, &p_id) == 2) {
    this->my_cmd_id_ = 0x110 + p_id;
    this->my_stat_id_ = 0x210 + p_id;
  }

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(gpio_num, gpio_num, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = { .brp = 160, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = true };
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); 

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    twai_start();
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT_OUTPUT_OD);
    this->initialized_ = true;
  }
}

void CANHub::loop() {
  if (!this->initialized_) return;
  twai_message_t message;
  while (twai_receive(&message, 0) == ESP_OK) {
    if (!message.extd && message.data_length_code >= 2) {
      uint8_t index = message.data;
      uint8_t type = message.data[1];
      if (index == 0xFF && type == 0xFF && message.identifier == this->my_cmd_id_) {
        if (wifi::global_wifi_component != nullptr) {
          wifi::global_wifi_component->start();
          wifi::global_wifi_component->resume();
          this->send_frame(this->my_stat_id_, 0xFF, 0xFF, {0x01});
        }
        continue;
      }
      std::vector<uint8_t> data(message.data + 2, message.data + message.data_length_code);
      for (auto *entity : this->entities_) {
        entity->on_frame(message.identifier, index, type, data);
      }
    }
  }
}

bool CANHub::send_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) {
  if (!this->initialized_) return false;
  twai_message_t message;
  message.identifier = can_id; message.extd = 0; message.rtr = 0;
  message.data_length_code = std::min((size_t)8, 2 + data.size());
  message.data = index; message.data[1] = type;
  for (size_t i = 0; i < data.size() && i < 6; i++) message.data[2 + i] = data[i];
  return twai_transmit(&message, pdMS_TO_TICKS(10)) == ESP_OK;
}

}
}