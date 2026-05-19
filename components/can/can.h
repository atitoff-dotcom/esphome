#pragma once
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "driver/twai.h"
#include <vector>

namespace esphome {
namespace can {

class CANEntity {
 public:
  void set_parent(class CANHub *parent) { parent_ = parent; }
  void set_can_ids(uint32_t cmd_id, uint32_t stat_id) { cmd_id_ = cmd_id; stat_id_ = stat_id; is_gateway_ = true; }
  void set_meta(uint8_t index, uint8_t type) { index_ = index; type_ = type; }
  virtual void on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) = 0;
 protected:
  class CANHub *parent_;
  uint8_t index_{0}; uint8_t type_{0}; uint32_t cmd_id_{0}; uint32_t stat_id_{0}; bool is_gateway_{false};
};

class CANHub : public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void register_listener(CANEntity *entity) { this->entities_.push_back(entity); }
  void setup() override;
  void loop() override;
  bool send_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data);
  int sw_idx{0}; int bs_idx{0}; int sn_idx{0};
  bool is_my_command_id(uint32_t can_id) { return can_id == this->my_cmd_id_; }
 protected:
  InternalGPIOPin *pin_{nullptr}; uint32_t bitrate_{25000}; uint32_t my_cmd_id_{0}; uint32_t my_stat_id_{0};
  bool initialized_{false}; std::vector<CANEntity *> entities_;
};

}
}