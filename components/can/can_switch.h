#pragma once
#include "esphome/components/switch/switch.h"
#include "can.h"

namespace esphome {
namespace can {

class CANSwitch : public switch_::Switch, public Component, public CANEntity {
 public:
  void set_listen_source(uint32_t listen_id, uint8_t src_idx, uint8_t src_type) {
    this->listen_id_ = listen_id; this->src_index_ = src_idx; this->src_type_ = src_type; this->has_p2p_ = true;
  }
  void setup() override { this->parent_->register_listener(this); }
  void on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) override;
 protected:
  void write_state(bool state) override;
  bool has_p2p_{false}; uint32_t listen_id_{0}; uint8_t src_index_{0}; uint8_t src_type_{0};
};

}
}