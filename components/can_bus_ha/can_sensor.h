#pragma once
#include "esphome/components/sensor/sensor.h"
#include "can.h"
#include <cstring>
#include <cmath>

namespace esphome {
namespace can_bus_ha {

class CANSensor : public sensor::Sensor, public Component, public CANEntity {
 public:
  void set_min_send_interval(uint32_t interval) { this->min_send_interval_ = interval; }
  void set_send_delta(float delta) { this->send_delta_ = delta; }

  void setup() override {
    this->parent_->register_listener(this);
    
    // Автоматическая отправка значения в CAN-шину при изменении (для периферии)
    if (!this->is_gateway_) {
      this->add_on_state_callback([this](float state) {
        uint32_t now = millis();
        // Проверяем минимальный интервал времени
        if (this->has_sent_ && (now - this->last_send_time_ < this->min_send_interval_)) return;
        
        // Проверяем дельту изменения значения
        if (this->has_sent_ && (std::abs(state - this->last_sent_val_) < this->send_delta_)) return;

        uint8_t data[4];
        std::memcpy(data, &state, sizeof(float));
        std::vector<uint8_t> vec(data, data + sizeof(float));
        if (this->parent_->send_frame(this->stat_id_, this->index_, this->type_, vec)) {
          this->last_send_time_ = now;
          this->last_sent_val_ = state;
          this->has_sent_ = true;
        }
      });
    }
  }

  void on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) override {
    if (data.size() < sizeof(float)) return;
    // На шлюзе парсим float из полученного статуса от периферии
    if (this->is_gateway_ && can_id == this->stat_id_ && index == this->index_ && type == this->type_) {
      float val;
      std::memcpy(&val, data.data(), sizeof(float));
      this->publish_state(val);
    }
  }

 protected:
  uint32_t min_send_interval_{0};
  float send_delta_{0.0f};
  uint32_t last_send_time_{0};
  float last_sent_val_{0.0f};
  bool has_sent_{false};
};

}
}
