#include "can_switch.h"

namespace esphome {
namespace can {

void CANSwitch::write_state(bool state) {
  uint8_t val = state ? 0x01 : 0x00;
  if (this->is_gateway_) {
    this->parent_->send_frame(this->cmd_id_, this->index_, this->type_, {val});
  } else {
    if (this->parent_->send_frame(this->stat_id_, this->index_, this->type_, {val})) this->publish_state(state);
  }
}

void CANSwitch::on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) {
  if (data.empty()) return;
  if (this->is_gateway_ && can_id == this->stat_id_ && index == this->index_ && type == this->type_) {
    this->publish_state(data[0] == 0x01);
  }
  if (!this->is_gateway_) {
    if (can_id == this->cmd_id_ && index == this->index_ && type == this->type_) this->publish_state(data[0] == 0x01);
    if (this->has_p2p_ && can_id == this->listen_id_ && index == this->src_index_ && type == this->src_type_) {
      bool target = (data[0] == 0x01);
      this->publish_state(target);
      this->parent_->send_frame(this->stat_id_, this->index_, this->type_, {data[0]});
    }
  }
}

}
}