import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@custom_embedded"]

can_od_c_ns = cg.esphome_ns.namespace("can_od_c")
CANODComponent = can_od_c_ns.class_("CANODComponent", cg.Component)
CANODTrigger = can_od_c_ns.class_("CANODTrigger", automation.Trigger)

CONF_PIN_TX = "pin_tx"
CONF_PIN_RX = "pin_rx"
CONF_ON_FRAME = "on_frame"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CANODComponent),
    cv.Required(CONF_PIN_TX): cv.int_,
    cv.Required(CONF_PIN_RX): cv.int_,
    cv.Optional(CONF_ON_FRAME): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CANODTrigger),
    }),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.pin_tx_num, config[CONF_PIN_TX])
    cg.add(var.pin_rx_num, config[CONF_PIN_RX])

    if CONF_ON_FRAME in config:
        for conf in config[CONF_ON_FRAME]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, cg.TemplateArguments(cg.uint32, cg.std_vector.template(cg.uint8)), conf)
