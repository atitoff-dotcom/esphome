#pragma once
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "driver/twai.h"
#include <vector>
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace can_bus_ha {

class CANEntity {
 public:
  void set_parent(class CANHub *parent) { parent_ = parent; }
  void set_can_ids(uint32_t cmd_id, uint32_t stat_id) { cmd_id_ = cmd_id; stat_id_ = stat_id; is_gateway_ = true; }
  void set_meta(uint8_t index, uint8_t type) { index_ = index; type_ = type; }
  void set_listen_source(uint32_t listen_id, uint8_t src_idx, uint8_t src_type) {
    this->listen_id_ = listen_id; this->src_index_ = src_idx; this->src_type_ = src_type; this->has_p2p_ = true;
  }
  virtual void on_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data) = 0;
  
  uint8_t get_index() const { return this->index_; }
  uint8_t get_type() const { return this->type_; }
  uint32_t get_cmd_id() const { return this->cmd_id_; }
  bool is_gateway() const { return this->is_gateway_; }

 protected:
  class CANHub *parent_;
  uint8_t index_{0}; uint8_t type_{0}; uint32_t cmd_id_{0}; uint32_t stat_id_{0}; bool is_gateway_{false};
  bool has_p2p_{false}; uint32_t listen_id_{0}; uint8_t src_index_{0}; uint8_t src_type_{0};
};

class CANHub : public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_gateway_id(uint32_t gateway_id) { this->gateway_id_ = gateway_id; }
  void set_peripheral_id(uint32_t peripheral_id) { this->peripheral_id_ = peripheral_id; }
  void register_listener(CANEntity *entity) { this->entities_.push_back(entity); }
  void setup() override;
  void loop() override;
  bool send_frame(uint32_t can_id, uint8_t index, uint8_t type, const std::vector<uint8_t> &data);
  int sw_idx{0}; int bs_idx{0}; int sn_idx{0}; int num_idx{0};
  bool is_my_command_id(uint32_t can_id) { return can_id == this->my_cmd_id_; }
  
  void set_wifi_status_sensor(text_sensor::TextSensor *sensor) { this->wifi_status_sensor_ = sensor; }
  void set_active_wifi_p_id(uint32_t p_id);
  void clear_active_wifi_p_id(uint32_t p_id);
  bool check_wifi_exclusivity(uint32_t target_cmd_id);

 protected:
  InternalGPIOPin *pin_{nullptr}; uint32_t bitrate_{25000}; uint32_t my_cmd_id_{0}; uint32_t my_stat_id_{0};
  uint32_t gateway_id_{0}; uint32_t peripheral_id_{0};
  bool initialized_{false}; std::vector<CANEntity *> entities_;
  text_sensor::TextSensor *wifi_status_sensor_{nullptr};
};

}
}