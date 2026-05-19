import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME
from . import can_ns, setup_peripheral_platform_template, CONF_BIND_TO

CANSensor = can_ns.class_('CANSensor', sensor.Sensor, cg.Component)
CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CANSensor),
    cv.GenerateID("can_hub_id"): cv.use_id(can_ns.class_('CANHub')),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_BIND_TO): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    await setup_peripheral_platform_template(config, CANSensor, await sensor.register_sensor, 0x03, "sn_idx")