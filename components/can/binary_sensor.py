import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_NAME
from . import can_ns, setup_peripheral_platform_template, CONF_BIND_TO

CANBinarySensor = can_ns.class_('CANBinarySensor', binary_sensor.BinarySensor, cg.Component)
CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CANBinarySensor),
    cv.GenerateID("can_hub_id"): cv.use_id(can_ns.class_('CANHub')),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_BIND_TO): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    await setup_peripheral_platform_template(config, CANBinarySensor, await binary_sensor.register_binary_sensor, 0x02, "bs_idx")