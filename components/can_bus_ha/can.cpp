#include "can.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"
#include "can_switch.h"

namespace esphome {
namespace can_bus_ha {

static const char *const TAG = "can.hub";

void CANHub::setup() {
  if (this->pin_ == nullptr) return;
  this->pin_->setup();
  gpio_num_t gpio_num = (gpio_num_t) this->pin_->get_pin();

  if (this->peripheral_id_ != 0) {
    this->my_cmd_id_ = 0x110 + this->peripheral_id_;
    this->my_stat_id_ = 0x210 + this->peripheral_id_;
  }

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(gpio_num, gpio_num, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = { .brp = 160, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = true };
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); 

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    twai_start();
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT_OUTPUT_OD);
    this->initialized_ = true;
    // Сообщаем шлюзу при старте, что WiFi на периферии выключен и гасим его локально
    if (this->my_stat_id_ != 0) {
      this->send_frame(this->my_stat_id_, 0xFF, 0xFF, {0x00});
      if (wifi::global_wifi_component != nullptr) {
        wifi::global_wifi_component->disable();
      }
    }
    // Инициализируем статус-сенсор WiFi шлюза
    if (this->wifi_status_sensor_ != nullptr) {
      this->wifi_status_sensor_->publish_state("Свободно");
    }
  }
}

void CANHub::loop() {
  if (!this->initialized_) return;
  twai_message_t message;
  while (twai_receive(&message, 0) == ESP_OK) {
    if (!message.extd && message.data_length_code >= 2) {
      uint8_t index = message.data[0];
      uint8_t type = message.data[1];
      if (index == 0xFF && type == 0xFF && message.identifier == this->my_cmd_id_) {
        if (wifi::global_wifi_component != nullptr) {
          if (message.data_length_code >= 3 && message.data[2] == 0x01) {
            wifi::global_wifi_component->enable();
            this->send_frame(this->my_stat_id_, 0xFF, 0xFF, {0x01});
          } else {
            wifi::global_wifi_component->disable();
            this->send_frame(this->my_stat_id_, 0xFF, 0xFF, {0x00});
          }
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
  message.data[0] = index; message.data[1] = type;
  for (size_t i = 0; i < data.size() && i < 6; i++) message.data[2 + i] = data[i];
  return twai_transmit(&message, pdMS_TO_TICKS(10)) == ESP_OK;
}

void CANHub::set_active_wifi_p_id(uint32_t p_id) {
  if (this->wifi_status_sensor_ != nullptr) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Активен peref%u-%u", this->gateway_id_, p_id);
    this->wifi_status_sensor_->publish_state(buf);
  }
}

void CANHub::clear_active_wifi_p_id(uint32_t p_id) {
  if (this->wifi_status_sensor_ != nullptr) {
    bool any_active = false;
    for (auto *entity : this->entities_) {
      if (entity->is_gateway() && entity->get_index() == 0xFF && entity->get_type() == 0xFF) {
        auto *sw = static_cast<CANSwitch *>(entity);
        if (sw->state && (entity->get_cmd_id() - 0x110 != p_id)) {
          any_active = true;
          break;
        }
      }
    }
    if (!any_active) {
      this->wifi_status_sensor_->publish_state("Свободно");
    }
  }
}

bool CANHub::check_wifi_exclusivity(uint32_t target_cmd_id) {
  for (auto *entity : this->entities_) {
    if (entity->is_gateway() && entity->get_index() == 0xFF && entity->get_type() == 0xFF) {
      if (entity->get_cmd_id() != target_cmd_id) {
        auto *sw = static_cast<CANSwitch *>(entity);
        if (sw->state) {
          return false;
        }
      }
    }
  }
  return true;
}

}
}