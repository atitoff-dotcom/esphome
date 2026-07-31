#pragma once
#include "esphome/components/number/number.h"
#include "can.h"
#include <cstring>

namespace esphome {
namespace can_bus_ha {

class CANNumber : public number::Number, public Component, public CANEntity {
 public:
  void setup() override {
    this->parent_->register_listener(this);
    if (!this->is_gateway_) {
      this->add_on_state_callback([this](float state) {
        if (this->boot_handshake_complete_) {
          uint8_t data[4];
          std::memcpy(data, &state, sizeof(float));
          std::vector<uint8_t> vec(data, data + sizeof(float));
          this->parent_->send_frame(this->stat_id_, this->index_, this->type_, vec);
        }
      });

      // Отправляем запрос актуального состояния в HA при старте
      this->parent_->send_frame(this->stat_id_, this->index_, this->type_, {0x02});

      // Таймаут 5 секунд на получение ответа
      this->set_timeout("boot_handshake", 5000, [this]() {
        this->boot_handshake_complete_ = true;
      });
    }
  }

  void control(float value) override {
    if (this->is_gateway_) {
      uint8_t data[4];
      std::memcpy(data, &value, sizeof(float));
      std::vector<uint8_t> vec(data, data + sizeof(float));
      this->parent_->send_frame(this->cmd_id_, this->index_, this->type_, vec);
    } else {
      this->publish_state(value);
    }
  }

  void on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) override {
    if (data.empty()) return;

    // Логика шлюза
    if (this->is_gateway_ && can_id == this->stat_id_ && index == this->index_ && type == this->type_) {
      if (data[0] == 0x02 && data.size() == 1) { // Запрос от периферии при ее старте
        float current_val = this->state;
        uint8_t reply_data[4];
        std::memcpy(reply_data, &current_val, sizeof(float));
        std::vector<uint8_t> vec(reply_data, reply_data + sizeof(float));
        this->parent_->send_frame(this->cmd_id_, this->index_, this->type_, vec);
      } else if (data.size() >= sizeof(float)) {
        float val;
        std::memcpy(&val, data.data(), sizeof(float));
        this->publish_state(val);
      }
    }

    // Логика периферии
    if (!this->is_gateway_) {
      if (can_id == this->cmd_id_ && index == this->index_ && type == this->type_ && data.size() >= sizeof(float)) {
        float val;
        std::memcpy(&val, data.data(), sizeof(float));
        this->publish_state(val);
        if (!this->boot_handshake_complete_) {
          this->boot_handshake_complete_ = true;
          this->cancel_timeout("boot_handshake");
        }
      }
      if (this->has_p2p_ && can_id == this->listen_id_ && index == this->src_index_ && type == this->src_type_ && data.size() >= sizeof(float)) {
        float val;
        std::memcpy(&val, data.data(), sizeof(float));
        this->publish_state(val);
      }
    }
  }

 protected:
  bool boot_handshake_complete_{false};
};

}
}
