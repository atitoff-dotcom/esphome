# esphome

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
