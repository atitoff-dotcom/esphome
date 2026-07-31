#include "can_switch.h"

namespace esphome {
namespace can_bus_ha_c {

void CANSwitch::setup() {
  this->parent_->register_listener(this);
  if (!this->is_gateway_) {
    this->add_on_state_callback([this](bool state) {
      if (this->boot_handshake_complete_) {
        uint8_t val = state ? 0x01 : 0x00;
        this->parent_->send_frame(this->stat_id_, this->index_, this->type_, {val});
      }
    });

    // Отправляем запрос актуального состояния в HA при старте
    this->parent_->send_frame(this->stat_id_, this->index_, this->type_, {0x02});

    // Таймаут 5 секунд на получение ответа от шлюза (если он выключен)
    this->set_timeout("boot_handshake", 5000, [this]() {
      this->boot_handshake_complete_ = true;
    });
  }
}

void CANSwitch::write_state(bool state) {
  if (this->is_gateway_) {
    uint8_t val = state ? 0x01 : 0x00;
    if (this->index_ == 0xFF && this->type_ == 0xFF) {
      if (state) {
        if (!this->parent_->check_wifi_exclusivity(this->cmd_id_)) {
          ESP_LOGW("can", "WiFi activation rejected: another module's WiFi is already active!");
          this->publish_state(false);
          return;
        }
        this->parent_->set_active_wifi_p_id(this->cmd_id_ - 0x110);
      } else {
        this->parent_->clear_active_wifi_p_id(this->cmd_id_ - 0x110);
      }
    }
    this->parent_->send_frame(this->cmd_id_, this->index_, this->type_, {val});
  } else {
    this->publish_state(state);
  }
}

void CANSwitch::on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) {
  if (data.empty()) return;
  if (this->is_gateway_ && can_id == this->stat_id_ && index == this->index_ && type == this->type_) {
    if (data[0] == 0x02) { // Запрос от периферии при ее старте
      uint8_t current_val = this->state ? 0x01 : 0x00;
      this->parent_->send_frame(this->cmd_id_, this->index_, this->type_, {current_val});
    } else {
      bool state = data[0] == 0x01;
      this->publish_state(state);
      if (this->index_ == 0xFF && this->type_ == 0xFF) {
        if (state) {
          this->parent_->set_active_wifi_p_id(this->stat_id_ - 0x210);
        } else {
          this->parent_->clear_active_wifi_p_id(this->stat_id_ - 0x210);
        }
      }
    }
  }
  if (!this->is_gateway_) {
    if (can_id == this->cmd_id_ && index == this->index_ && type == this->type_) {
      this->publish_state(data[0] == 0x01);
      if (!this->boot_handshake_complete_) {
        this->boot_handshake_complete_ = true;
        this->cancel_timeout("boot_handshake");
      }
    }
    if (this->has_p2p_ && can_id == this->listen_id_ && index == this->src_index_ && type == this->src_type_) {
      if (this->src_type_ == 0x02) { // binary_sensor (button)
        if (data[0] == 0x01) { // Toggle on press only
          this->publish_state(!this->state);
        }
      } else { // Switch or others - mirror state
        this->publish_state(data[0] == 0x01);
      }
    }
  }
}

}
}