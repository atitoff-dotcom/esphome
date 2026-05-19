import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, CONF_NAME
from . import can_ns, setup_peripheral_platform_template, CONF_BIND_TO

CANSwitch = can_ns.class_('CANSwitch', switch.Switch, cg.Component)
CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CANSwitch),
    cv.GenerateID("can_hub_id"): cv.use_id(can_ns.class_('CANHub')),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_BIND_TO): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    await setup_peripheral_platform_template(config, CANSwitch, await switch.register_switch, 0x01, "sw_idx")