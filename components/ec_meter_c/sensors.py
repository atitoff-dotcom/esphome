import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT

CODEOWNERS = ["@custom_embedded"]

ec_meter_c_ns = cg.esphome_ns.namespace("ec_meter_c")
ECMeterCSensor = ec_meter_c_ns.class_(
    "ECMeterCSensor", sensor.Sensor, cg.PollingComponent
)

CONF_PIN_DRIVE = "pin_drive"
CONF_ADC_CHANNEL = "adc_channel"
CONF_R_BALANCE = "r_balance"
CONF_K_CELL = "k_cell"
CONF_TEMPERATURE_SENSOR_ID = "temperature_sensor_id"

CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="µS/cm",
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
).extend({
    cv.GenerateID(): cv.declare_id(ECMeterCSensor),
    cv.Required(CONF_PIN_DRIVE): cv.int_,
    cv.Required(CONF_ADC_CHANNEL): cv.int_,
    cv.Required(CONF_TEMPERATURE_SENSOR_ID): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_R_BALANCE, default=1000.0): cv.float_,
    cv.Optional(CONF_K_CELL, default=1.0): cv.float_,
}).extend(cv.polling_component_schema("2s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.pin_drive_num, config[CONF_PIN_DRIVE])
    cg.add(var.adc_channel_num, config[CONF_ADC_CHANNEL])
    cg.add(var.r_balance, config[CONF_R_BALANCE])
    cg.add(var.k_cell, config[CONF_K_CELL])

    temp_sens = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR_ID])
    cg.add(var.temperature_sensor, temp_sens)
