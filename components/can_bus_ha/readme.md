# Компонент CAN Bus Home Assistant (can_bus_ha)

Кастомный компонент ESPHome для построения распределенной сети умного дома на шине CAN (1-wire или 2-wire) с автоматической интеграцией в Home Assistant.

## 🌟 Основные особенности
- **Динамическая автосборка шлюза**: Шлюз (`gateway-1`) сканирует YAML-файлы периферийных модулей и сам регистрирует их сущности в Home Assistant. Не нужно менять прошивку шлюза при добавлении новых модулей или сенсоров.
- **Peer-to-Peer (P2P) связи**: Периферийные модули общаются между собой напрямую без участия шлюза и центрального сервера (например, кнопка на одном модуле переключает реле на другом).
- **Синхронизация состояний при старте (Handshake)**: Модули восстанавливают состояние реле/настроек локально из флэш-памяти (NVS), но одновременно запрашивают актуальный статус у HA. Если HA выключен, модули работают полностью автономно.
- **Принцип примитивов**: Вместо разработки C++ кода под каждый датчик или исполнитель, библиотека использует 4 базовых примитива (`switch`, `binary_sensor`, `sensor`, `number`). Из них в Home Assistant можно собрать любое сложное устройство (диммеры, климат, шторы и т.д.).

---

## 🛠 Архитектурные примитивы и типы данных
Данные передаются стандартными 11-битными CAN кадрами (длина до 8 байт).
- **Байт 0**: `Index` (индекс сущности данного типа на модуле, `0-255`).
- **Байт 1**: `Type` (код домена).
- **Байты 2-7**: Полезная нагрузка.

| Код (`Type`) | Платформа | Формат данных в CAN | Применение |
| :--- | :--- | :--- | :--- |
| **`0x01`** | `switch` | 1 байт (`0x00` = Выкл, `0x01` = Вкл) | Реле, клапаны, контакторы |
| **`0x02`** | `binary_sensor` | 1 байт (`0x00` = Отпущено, `0x01` = Нажато) | Физические кнопки, герконы, датчики движения |
| **`0x03`** | `sensor` | 4 байта (`float` IEEE 754) | Температура, влажность, энергопотребление |
| **`0x04`** | `number` | 4 байта (`float` IEEE 754) | Диммеры, уставки температуры, положения штор |

---

## ⚙️ Конфигурация компонента и защита от флуда

### 1. Настройка адресации (Хаб CAN)
Для каждого устройства на шине CAN необходимо явно прописать его роль и адреса в блоке `can_bus_ha`:

```yaml
# Конфигурация для Шлюза (gateway-1) на плате WT32-ETH01 (Ethernet)
esphome:
  name: gateway-1

esp32:
  board: wt32-eth01
  framework:
    type: esp-idf

# Настройка встроенного Ethernet LAN8720 на WT32-ETH01
ethernet:
  type: LAN8720
  mdc_pin: GPIO23
  mdio_pin: GPIO18
  clk_mode: GPIO0_IN
  phy_addr: 1

can_bus_ha:
  id: my_gateway_hub
  pin: GPIO5            # GPIO5 (свободный пин) для 1-wire CAN
  gateway_id: 1        # Уникальный ID шлюза
```

# Конфигурация для Периферийного модуля (peref)
can_bus_ha:
  id: my_can_bus
  pin: GPIO5
  gateway_id: 1        # К какому шлюзу подключена шина
  peripheral_id: 2     # Уникальный ID модуля на данной шине (1-255)
  bitrate: 25000       # Скорость шины CAN (по умолчанию 25000)
```

### 2. Защита шины от флуда датчиков (Антифлуд)
Аналоговые сенсоры (температура, напряжение) могут часто менять значения из-за шумов АЦП, перегружая шину CAN. Для примитива `sensor` предусмотрены фильтры, работающие на уровне отправки в шину:

```yaml
sensor:
  - platform: can_bus_ha
    id: room_temperature
    name: "Room Temperature"
    min_send_interval: 5s  # Отправлять в CAN не чаще раза в 5 секунд
    send_delta: 0.1        # Отправлять в CAN только если значение изменилось на 0.1 или более
```

---

## 📋 Практические примеры реализации

Вся прелесть архитектуры примитивов заключается в том, что на периферийных модулях мы объявляем только базовые сущности (`can_bus_ha`), а на стороне Home Assistant собираем их в красивые интерфейсные шаблоны.

### Пример 1: Управление диммируемым светом (диммер)
Свет управляется двумя примитивами: Вкл/Выкл (`switch`) и Яркость 0-100% (`number`).

#### Конфигурация на периферийном модуле (Устройство 3):
```yaml
# can_bus_ha/tst/perefn-1-2.yaml
can_bus_ha:
  id: my_can_bus
  pin: GPIO5

switch:
  - platform: can_bus_ha
    id: light_on_off
    name: "Livingroom Light Power"
    on_turn_on:  [- gpio.digital_write: { pin: GPIO4, value: high }]
    on_turn_off: [- gpio.digital_write: { pin: GPIO4, value: low }]

number:
  - platform: can_bus_ha
    id: light_brightness
    name: "Livingroom Light Brightness"
    min_value: 0
    max_value: 255
    step: 1
    # Логика диммирования (например, вывод ШИМ на GPIO15)
    on_value:
      then:
        - ledc.set_level:
            id: my_ledc
            level: !lambda 'return x / 255.0;'

ledc:
  - pin: GPIO15
    id: my_ledc
```

#### Виртуальный диммер в Home Assistant (`configuration.yaml`):
```yaml
light:
  - platform: template
    lights:
      livingroom_dimmer:
        friendly_name: "Диммер в гостиной"
        value_template: "{{ is_state('switch.peref1_2_light_on_off', 'on') }}"
        level_template: "{{ (states('number.peref1_2_light_brightness') | float) }}"
        turn_on:
          action: switch.turn_on
          target:
            entity_id: switch.peref1_2_light_on_off
        turn_off:
          action: switch.turn_off
          target:
            entity_id: switch.peref1_2_light_on_off
        set_level:
          action: number.set_value
          target:
            entity_id: number.peref1_2_light_brightness
          data:
            value: "{{ brightness }}"
```

---

### Пример 2: Управление роллетами / рулонными шторами (Cover)
Шторы управляются примитивом `number` для задания положения от 0 (закрыто) до 100 (открыто).

#### Конфигурация на периферийном модуле:
```yaml
# cover_peripheral.yaml
number:
  - platform: can_bus_ha
    id: cover_position
    name: "Bedroom Cover Position"
    min_value: 0
    max_value: 100
    step: 1
    on_value:
      then:
        # Логика физического движения мотора шторы до координаты x
        - lambda: 'id(my_motor).move_to(x);'
```

#### Виртуальная штора в Home Assistant (`configuration.yaml`):
```yaml
cover:
  - platform: template
    covers:
      bedroom_cover:
        device_class: shade
        friendly_name: "Рулонная штора в спальне"
        position_template: "{{ states('number.peref1_2_cover_position') | int }}"
        open_cover:
          action: number.set_value
          target:
            entity_id: number.peref1_2_cover_position
          data:
            value: 100
        close_cover:
          action: number.set_value
          target:
            entity_id: number.peref1_2_cover_position
          data:
            value: 0
        set_cover_position:
          action: number.set_value
          target:
            entity_id: number.peref1_2_cover_position
          data:
            value: "{{ position }}"
```

---

### Пример 3: Умный климат-контроль (Dallas + Термостат)
Климат считывает температуру датчиком `dallas` и передает её через `sensor`. Уставка температуры задается через `number`.

#### Конфигурация на периферийном модуле:
```yaml
# climate_peripheral.yaml
dallas:
  - pin: GPIO12

sensor:
  # 1. Физический датчик температуры
  - platform: dallas
    id: ds18b20_temp
    address: 0x28000007133742
    on_value:
      then:
        - sensor.template.publish:
            id: room_temperature
            state: !lambda 'return x;'

  # 2. CAN-транспорт для отправки температуры в HA
  - platform: can_bus_ha
    id: room_temperature
    name: "Room Current Temperature"

# 3. CAN-транспорт для получения целевой температуры (уставки) от HA
number:
  - platform: can_bus_ha
    id: target_temperature
    name: "Room Target Temperature"
    min_value: 16
    max_value: 30
    step: 0.5
```

#### Виртуальный термостат в Home Assistant (`configuration.yaml`):
```yaml
climate:
  - platform: generic_thermostat
    name: "Отопление Детская"
    heater: switch.peref1_2_heater_relay  # Другое CAN-реле нагревателя
    target_sensor: sensor.peref1_2_room_temperature
    min_temp: 16
    max_temp: 30
    target_temp_step: 0.5
    # Связываем ползунок уставки
    target_temp: "{{ states('number.peref1_2_target_temperature') | float }}"
```

---

### Пример 4: Прямая P2P связь (Свет по кнопке без сервера)
Физическая кнопка на Устройстве 2 (`peref1-1-1`) переключает свет на Устройстве 3 (`perefn-1-2`) без участия шлюза и Home Assistant.

#### Координатор кнопки (Устройство 2):
```yaml
# components/can_bus_ha/tst/peref1-1-1.yaml
binary_sensor:
  - platform: can_bus_ha
    id: wall_button
    pin: { number: GPIO10, mode: INPUT_PULLUP, inverted: true }
```

#### Исполнитель реле (Устройство 3):
```yaml
# components/can_bus_ha/tst/perefn-1-2.yaml
switch:
  - platform: can_bus_ha
    id: relay_1
    bind_to: wall_button   # <--- Автоматически подпишется на статус кнопки с Устройства 2
    on_turn_on:  [- gpio.digital_write: { pin: GPIO4, value: high }]
    on_turn_off: [- gpio.digital_write: { pin: GPIO4, value: low }]
```

---

## 🔌 Схемы физического подключения CAN-трансиверов

Компонент работает с драйвером TWAI процессоров ESP32. Возможны два варианта физической шины:

### Вариант 1: Стандартная двухпроводная шина CAN (со стандартными трансиверами типа TJA1050, SN65HVD230)
- **ESP32 TX Pin (GPIO5)** подключается к **TXD** трансивера.
- **ESP32 RX Pin (GPIO4)** подключается к **RXD** трансивера.
- Шина ведется витой парой (CAN_H, CAN_L). На концах линии ставятся терминаторы 120 Ом.

### Вариант 2: Однопроводная CAN-шина (через Open-Drain)
Позволяет соединить несколько ESP32 по одному проводу + земля без трансиверов:
- В коде используется один пин **GPIO5** как для TX, так и для RX.
- Пин конфигурируется в режиме Open Drain.
- Все GPIO5 модулей соединяются между собой одним общим сигнальным проводом.
- На шину ставится один общий подтягивающий резистор 1 кОм к питанию 3.3В.

---

## 📶 Удаленная прошивка по WiFi (OTA) и поддержка нескольких шлюзов

### 1. Как работает удаленная прошивка через CAN и защита от шторма
По умолчанию на периферийных модулях WiFi отключен при старте для снижения энергопотребления, электромагнитного шума и повышения стабильности:
```yaml
esphome:
  on_boot:
    priority: 200.0
    then: [- lambda: 'wifi::global_wifi_component->stop();']
```

Для прошивки модуля "по воздуху" (OTA) выполните следующие шаги:
1. Шлюз автоматически генерирует в Home Assistant виртуальный переключатель для каждого модуля: `switch.peref{gateway_id}_{p_id}_wifi` (например, `switch.peref1_2_wifi`).
2. При включении этого свитча в HA шлюз отправляет специальный CAN-кадр с метаданными `index = 0xFF`, `type = 0xFF`, `data = {0x01}`.
3. Периферийное устройство ловит этот кадр, запускает WiFi-модуль (`wifi::global_wifi_component->start()`, `resume()`) и возвращает статус подтверждения шлюзу, после чего свитч в HA переходит в состояние «Включен».
4. Вы выполняете беспроводную OTA-прошивку модуля из ESPHome.
5. После успешной прошивки или перезагрузки модуль стартует, его WiFi автоматически гасится (благодаря `on_boot` скрипту), а сам модуль отправляет шлюзу стартовый статус-кадр `0xFF 0xFF {0x00}`.
6. Получив данный статус, шлюз сбрасывает свитч в интерфейсе HA обратно в состояние «Выключен».

#### 🛡️ Защита от шторма и блокировки (Защита от дурака)
Включение WiFi одновременно на многих модулях может перегрузить Wi-Fi роутер и создать опасный скачок потребления тока на общей линии питания CAN. Для предотвращения этого в шлюзе реализована встроенная защита:
- **Блокировка одновременного включения**: В любой момент времени WiFi может быть включен **только на одном модуле**.
- Если вы пытаетесь включить WiFi на Модуле B, пока WiFi на Модуле A уже активен, шлюз заблокирует эту команду (не отправит CAN-кадр), а переключатель Модуля B в Home Assistant автоматически отскочит обратно в положение **«Выкл»**. Это гарантирует безопасность и защищает активную сессию прошивки на Модуле A от обрыва.
- **Информационный статус-сенсор**: Шлюз автоматически создает в HA текстовый сенсор `text_sensor.peref{gateway_id}_wifi_status` (например, `peref1 WiFi Status`). Он отображает состояние шины:
  - `Свободно` — когда все WiFi выключены;
  - `Активен peref1-2` — когда на периферии 2 включен WiFi, блокируя активацию других модулей.
- **Автоматическое появление в GUI**: Если все устройства шлюза добавлены в одну Область (Area) в HA, этот статус-сенсор и все свитчи WiFi будут автоматически сгруппированы и выведены на страницу области без необходимости ручного редактирования панелей Lovelace.

### 2. Поддержка нескольких независимых шлюзов
Архитектура поддерживает масштабирование на несколько независимых шлюзов со своими физическими CAN-шинами:
- Каждый шлюз имеет имя `gateway-{gateway_id}` (например, `gateway-1`, `gateway-2`).
- Периферийные модули именуются как `peref{gateway_id}-{subnet}-{p_id}` или `perefn-{gateway_id}-{p_id}`.
- При сборке конкретного шлюза, парсер на Python автоматически фильтрует файлы конфигураций и подключает к этому шлюзу только те периферийные модули, у которых `gateway_id` в имени совпадает с ID шлюза.
- Физическая изоляция CAN-шин шлюзов исключает любые конфликты адресов, позволяя использовать одинаковые адреса периферии `p_id` (например, `1`, `2`) на разных шинах.

