import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, CONF_NAME
from . import can_ns, setup_peripheral_platform_template, CONF_BIND_TO

CANNumber = can_ns.class_('CANNumber', number.Number, cg.Component)
CONFIG_SCHEMA = number.number_schema.extend({
    cv.GenerateID(): cv.declare_id(CANNumber),
    cv.GenerateID("can_hub_id"): cv.use_id(can_ns.class_('CANHub')),
    cv.Optional(CONF_NAME): cv.string,
    cv.Optional(CONF_BIND_TO): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    min_value = config.get("min_value", 0.0)
    max_value = config.get("max_value", 100.0)
    step = config.get("step", 1.0)
    
    async def reg_fn(var, config):
        await number.register_number(var, config, min_value=min_value, max_value=max_value, step=step)
        
    await setup_peripheral_platform_template(config, CANNumber, reg_fn, 0x04, "num_idx")
