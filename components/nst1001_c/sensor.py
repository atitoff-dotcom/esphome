import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT, UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE

CODEOWNERS = ["@custom_embedded"]

nst1001_c_ns = cg.esphome_ns.namespace("nst1001_c")
NST1001CSensor = nst1001_c_ns.class_(
    "NST1001CSensor", sensor.Sensor, cg.PollingComponent
)

CONF_PIN_DQ = "pin_dq"

CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
).extend({
    cv.GenerateID(): cv.declare_id(NST1001CSensor),
    cv.Required(CONF_PIN_DQ): cv.int_, # Номер GPIO для линии данных NST1001
}).extend(cv.polling_component_schema("2s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    
    cg.add(var.pin_dq_num, config[CONF_PIN_DQ])
