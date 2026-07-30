#pragma once
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "can.h"

namespace esphome {
namespace can_bus_ha {

class CANBinarySensor : public binary_sensor::BinarySensor, public Component, public CANEntity {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  
  void setup() override {
    if (this->is_gateway_) {
      this->parent_->register_listener(this);
      return;
    }
    
    if (this->pin_ != nullptr) {
      this->pin_->setup();
      this->publish_initial_state(this->pin_->digital_read());
      this->raw_state_ = this->pin_->digital_read();
    }
    
    this->parent_->register_listener(this);
    
    // Автоматическая отправка статуса в CAN-шину при изменении состояния
    this->add_on_state_callback([this](bool state) {
      this->parent_->send_frame(this->stat_id_, this->index_, this->type_, {state ? 0x01 : 0x00});
    });
  }

  void loop() override {
    if (this->is_gateway_) return;
    if (this->pin_ != nullptr) {
      bool raw_state = this->pin_->digital_read();
      uint32_t now = millis();
      if (raw_state != this->raw_state_) {
        this->last_change_ = now;
        this->raw_state_ = raw_state;
      }
      if (now - this->last_change_ >= 50) { // 50мс антидребезг
        if (raw_state != this->state) {
          this->publish_state(raw_state);
        }
      }
    }
  }

  void on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) override {
    if (data.empty()) return;
    // На шлюзе обновляем состояние при получении кадра статуса от периферии
    if (this->is_gateway_ && can_id == this->stat_id_ && index == this->index_ && type == this->type_) {
      this->publish_state(data[0] == 0x01);
    }
  }

 protected:
  InternalGPIOPin *pin_{nullptr};
  bool raw_state_{false};
  uint32_t last_change_{0};
};

}
}
