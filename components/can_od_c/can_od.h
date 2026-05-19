#ifndef CAN_OD_H
#define CAN_OD_H

#include "driver/twai.h"
#include "driver/gpio.h"
#include <vector>

#ifdef __cplusplus
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace can_od_c {

// Структура для передачи данных в лямбды ESPHome
struct CANODFrame {
    uint32_t can_id;
    std::vector<uint8_t> data;
};

class CANODComponent : public Component {
 public:
  uint32_t pin_tx_num;
  uint32_t pin_rx_num;
  
  // Колбэк для автоматизации при получении кадра
  Callback<void(uint32_t, std::vector<uint8_t>)> on_frame_callback;

  void setup() override {
    // Расчет таймингов под 25 кГц для тактовой частоты TWAI 80 МГц
    twai_timing_config_t t_config = {
        .brp = 200,           // Делитель: 80MHz / (16 * 25kHz) = 200
        .prop_seg = 7,        
        .phase_seg1 = 5,      
        .phase_seg2 = 3,      // Итого 1 + 7 + 5 + 3 = 16 TQ на бит
        .sjw = 3,             
        .triple_sampling = false
    };

    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = (gpio_num_t)pin_tx_num,
        .rx_io = (gpio_num_t)pin_rx_num,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 16,
        .rx_queue_len = 16,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0
    };

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        // Конфигурируем физический Pad пина TX в режим Open Drain
        gpio_config_t tx_pad_conf = {
            .pin_bit_mask = (1ULL << pin_tx_num),
            .mode = GPIO_MODE_INPUT_OUTPUT_OD, // Смена режима на открытый сток
            .pull_up_en = GPIO_PULLUP_DISABLE, // Полагаемся на внешний резистор 1 кОм
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&tx_pad_conf);

        // Перепривязываем сигналы TWAI обратно к матрице GPIO после реконфига Pad
        esp_rom_gpio_connect_out_signal((gpio_num_t)pin_tx_num, TWAI_TX_IDX, false, false);
        esp_rom_gpio_connect_in_signal((gpio_num_t)pin_rx_num, TWAI_RX_IDX, false);

        twai_start();
    }
  }

  void loop() override {
    twai_message_t message;
    // Неблокирующее чтение входящей очереди шины CAN (таймаут 0)
    if (twai_receive(&message, 0) == ESP_OK) {
        if (!(message.rtr)) { // Обрабатываем только Data-фреймы (не RTR)
            std::vector<uint8_t> frame_data;
            for (int i = 0; i < message.data_length_code; i++) {
                frame_data.push_back(message.data[i]);
            }
            // Передаем ID и массив байт в ESPHome автоматизацию
            this->on_frame_callback.call(message.identifier, frame_data);
        }
    }
  }

  // Метод для отправки данных (вызывается из YAML-лямбд)
  void send_frame(uint32_t can_id, const std::vector<uint8_t> &data) {
    twai_message_t message;
    message.identifier = can_id;
    message.extd = 0; // Стандартный 11-битный ID. Для 29-битного поставить 1
    message.rtr = 0;
    message.data_length_code = data.size() > 8 ? 8 : data.size();
    
    for (int i = 0; i < message.data_length_code; i++) {
        message.data[i] = data[i];
    }

    twai_transmit(&message, pdMS_TO_TICKS(10));
  }
};

// Обертка для триггера `on_frame` в YAML
class CANODTrigger : public Trigger<uint32_t, std::vector<uint8_t>> {
 public:
  explicit CANODTrigger(CANODComponent *parent) {
    parent->on_frame_callback.add([this](uint32_t can_id, std::vector<uint8_t> data) {
      this->trigger(can_id, data);
    });
  }
};

} // namespace can_od_c
} // namespace esphome
#endif

#endif // CAN_OD_H
