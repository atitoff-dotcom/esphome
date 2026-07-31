import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME
from . import can_ns, setup_peripheral_platform_template, CONF_BIND_TO

CANSensor = can_ns.class_('CANSensor', sensor.Sensor, cg.Component)
CONFIG_SCHEMA = sensor.sensor_schema.extend({
    cv.GenerateID(): cv.declare_id(CANSensor),
    cv.GenerateID("can_hub_id"): cv.use_id(can_ns.class_('CANHub')),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_BIND_TO): cv.string,
    cv.Optional("min_send_interval", default="0ms"): cv.positive_time_period_milliseconds,
    cv.Optional("send_delta", default=0.0): cv.float_,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await setup_peripheral_platform_template(config, CANSensor, sensor.register_sensor, 0x03, "sn_idx")
    if var is not None:
        cg.add(var.set_min_send_interval(config["min_send_interval"]))
        cg.add(var.set_send_delta(config["send_delta"]))