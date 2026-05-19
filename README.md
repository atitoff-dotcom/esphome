# esphome



### nst1001_c

```yaml
external_components:
  - source: github://atitoff-dotcom/esphome@main
    components: [ nst1001_c ]

sensor:
  # Датчик температуры NST1001
  - platform: nst1001_c
    id: water_temp_nst
    name: "Nutrient Temperature"
    pin_dq: 5
    update_interval: 2s
```

### can_od_c

```yaml
external_components:
  - source: github://atitoff-dotcom/esphome@main
    components: [ can_od_c ]

# Инициализируем наш кастомный CAN Open Drain на скорости 25 кГц
can_od_c:
  id: my_can_bus
  pin_tx: 6  # Подключаем GPIO6 и GPIO7 вместе + pull-up резистор 1 кОм на 3.3V
  pin_rx: 7
  
  # Логика приема пакетов из шины
  on_frame:
    then:
      - lambda: |-
          // x - это can_id (uint32_t), y - это массив данных (std::vector<uint8_t>)
          ESP_LOGI("can_custom", "Received frame ID: %d, Data len: %d", x, y.size());
          if (x == 0x100 && y.size() >= 2) {
              // Пример парсинга температуры, пришедшей от другой платы
              float temp = (int16_t)((y[0] << 8) | y[1]) / 10.0f;
              ESP_LOGI("can_custom", "Parsed temperature from bus: %.1f C", temp);
          }

# Триммер для тестов: шлем пакет раз в 5 секунд во внутреннюю сеть
interval:
  - interval: 5s
    then:
      - lambda: |-
          // Формируем пакет данных (максимум 8 байт)
          std::vector<uint8_t> packet;
          packet.push_back(0xAA);
          packet.push_back(0xBB);
          
          // Отправляем пакет с ID 0x100 через наш компонент
          id(my_can_bus).send_frame(0x100, packet);
```