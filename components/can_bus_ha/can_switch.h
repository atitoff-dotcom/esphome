#pragma once
#include "esphome/components/switch/switch.h"
#include "can.h"

namespace esphome {
namespace can_bus_ha {

class CANSwitch : public switch_::Switch, public Component, public CANEntity {
 public:
  void setup() override;
  void on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) override;
 protected:
  void write_state(bool state) override;
  bool boot_handshake_complete_{false};
};

}
}