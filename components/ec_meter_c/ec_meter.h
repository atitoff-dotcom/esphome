#ifndef EC_METER_H
#define EC_METER_H

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include <cmath>

#define FILTER_WINDOW 16
#define TEMP_COEFF_ALPHA 0.020f

typedef struct {
    gpio_num_t pin_drive;
    adc_channel_t adc_channel;
    adc_oneshot_unit_handle_t adc_handle;
    float r_balance;
    float k_cell;

    float buffer[FILTER_WINDOW];
    int head;
    float sum;
    int count;
    float last_ec;
} ECMeter;

static inline void ec_init(ECMeter *meter, gpio_num_t drive, adc_oneshot_unit_handle_t handle, adc_channel_t chan, float r_bal, float k) {
    meter->pin_drive = drive;
    meter->adc_handle = handle;
    meter->adc_channel = chan;
    meter->r_balance = r_bal;
    meter->k_cell = k;
    meter->head = 0;
    meter->sum = 0.0f;
    meter->count = 0;
    meter->last_ec = 0.0f;

    for(int i = 0; i < FILTER_WINDOW; i++) meter->buffer[i] = 0.0f;

    gpio_reset_pin(drive);
    gpio_set_direction(drive, GPIO_MODE_OUTPUT);
    gpio_set_level(drive, 0);
}

static inline float ec_apply_temp_compensation(float ec_raw, float temperature) {
    if (std::isnan(temperature) || temperature < -10.0f || temperature > 60.0f) {
        return ec_raw;
    }
    float compensation = 1.0f + TEMP_COEFF_ALPHA * (temperature - 25.0f);
    return ec_raw / compensation;
}

static inline float ec_measure_pulse(ECMeter *meter, float current_temperature) {
    int raw_adc = 0;

    gpio_set_level(meter->pin_drive, 1);
    esp_rom_delay_us(50);
    adc_oneshot_read(meter->adc_handle, meter->adc_channel, &raw_adc);
    gpio_set_level(meter->pin_drive, 0);
    esp_rom_delay_us(50);

    if (raw_adc <= 10 || raw_adc >= 4080) return 0.0f;

    float v_ratio = (float)raw_adc / 4095.0f;
    float r_solution = (meter->r_balance * (1.0f - v_ratio)) / v_ratio;

    if (r_solution <= 0) return 0.0f;

    float ec_raw = (1.0f / r_solution) * meter->k_cell * 1000000.0f;
    float ec_compensated = ec_apply_temp_compensation(ec_raw, current_temperature);

    // Moving Average
    if (meter->count == FILTER_WINDOW) {
        meter->sum -= meter->buffer[meter->head];
    } else {
        meter->count++;
    }

    meter->buffer[meter->head] = ec_compensated;
    meter->sum += ec_compensated;
    meter->head = (meter->head + 1) % FILTER_WINDOW;

    meter->last_ec = meter->sum / meter->count;
    return meter->last_ec;
}

#ifdef __cplusplus
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ec_meter_c {

class ECMeterCSensor : public sensor::Sensor, public PollingComponent {
 public:
  ECMeter meter;
  uint32_t pin_drive_num;
  uint32_t adc_channel_num;
  float r_balance;
  float k_cell;

  // Указатель на независимый сенсор температуры (им может быть наш NST1001)
  sensor::Sensor *temperature_sensor{nullptr};

  void setup() override {
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_config_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
    };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc1_handle, (adc_channel_t)adc_channel_num, &config);

    ec_init(&this->meter, (gpio_num_t)pin_drive_num, adc1_handle, (adc_channel_t)adc_channel_num, r_balance, k_cell);
  }

  void update() override {
    float current_temp = 25.0f;
    if (this->temperature_sensor != nullptr && this->temperature_sensor->has_state()) {
        current_temp = this->temperature_sensor->get_state();
    }

    float current_ec = 0;
    for(int i = 0; i < 5; i++) {
        current_ec = ec_measure_pulse(&this->meter, current_temp);
    }
    this->publish_state(current_ec);
  }
};

} // namespace ec_meter_c
} // namespace esphome
#endif

#endif // EC_METER_H
