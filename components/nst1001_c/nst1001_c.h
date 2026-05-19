#ifndef NST1001_C_H
#define NST1001_C_H

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include <cmath>

#ifdef __cplusplus
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace nst1001_c {

class NST1001CSensor : public sensor::Sensor, public PollingComponent {
 public:
  uint32_t pin_dq_num;
  pcnt_unit_handle_t pcnt_unit{nullptr};
  pcnt_channel_handle_t pcnt_chan{nullptr};

  void setup() override {
    // 1. Конфигурация юнита PCNT
    pcnt_unit_config_t unit_config = {
        .high_limit = 4000, // С запасом (максимум у датчика 3201 импульс)
        .low_limit = -10,
        .flags = { .accum_count = false }
    };
    pcnt_new_unit(&unit_config, &this->pcnt_unit);

    // 2. Конфигурация канала PCNT
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = (gpio_num_t)this->pin_dq_num,
        .level_gpio_num = -1, // Направление счета фиксировано
    };
    pcnt_new_channel(this->pcnt_unit, &chan_config, &this->pcnt_chan);

    // Считаем импульсы по спаду (Falling Edge)
    pcnt_channel_set_edge_action(this->pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    
    // Аппаратный Глитч-фильтр: игнорируем всплески короче 1000 нс (1 мкс)
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000, 
    };
    pcnt_unit_set_glitch_filter(this->pcnt_unit, &filter_config);

    // Активируем аппаратный блок
    pcnt_unit_enable(this->pcnt_unit);
  }

  void update() override {
    int pulse_count = 0;

    // Сброс и запуск аппаратного счетчика
    pcnt_unit_clear_count(this->pcnt_unit);
    pcnt_unit_start(this->pcnt_unit);

    // Задержка на время измерения датчика и пачки импульсов (~55 мс)
    esp_rom_delay_us(55000); 

    // Стоп и чтение регистра данных
    pcnt_unit_stop(this->pcnt_unit);
    pcnt_unit_get_count(this->pcnt_unit, &pulse_count);

    if (pulse_count < 0) pulse_count = -pulse_count;

    // Валидация данных по даташиту (0°C = 800 имп., 150°C = 3201 имп.)
    if (pulse_count < 10 || pulse_count > 3300) {
        this->publish_state(NAN);
        return;
    }

    // Формула: T = (Pulses / 16) - 50
    float temperature = ((float)pulse_count / 16.0f) - 50.0f;
    this->publish_state(temperature);
  }
};

} // namespace nst1001_c
} // namespace esphome
#endif

#endif // NST1001_C_H
