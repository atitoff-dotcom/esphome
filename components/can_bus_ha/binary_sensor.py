import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome import pins
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN
from . import can_ns, setup_peripheral_platform_template, CONF_BIND_TO

CANBinarySensor = can_ns.class_('CANBinarySensor', binary_sensor.BinarySensor, cg.Component)
CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CANBinarySensor),
    cv.GenerateID("can_hub_id"): cv.use_id(can_ns.class_('CANHub')),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_BIND_TO): cv.string,
    cv.Optional(CONF_PIN): pins.gpio_input_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await setup_peripheral_platform_template(config, CANBinarySensor, binary_sensor.register_binary_sensor, 0x02, "bs_idx")
    if var is not None and CONF_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))