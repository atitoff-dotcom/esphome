import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, yaml_util
from esphome.components import switch, binary_sensor, sensor
from esphome.const import CONF_ID, CONF_PIN, CONF_NAME
import os
import re

can_ns = cg.esphome_ns.namespace('can')
CANHub = can_ns.class_('CANHub', cg.Component)

CANSwitch = can_ns.class_('CANSwitch', switch.Switch, cg.Component)
CANBinarySensor = can_ns.class_('CANBinarySensor', binary_sensor.BinarySensor, cg.Component)
CANSensor = can_ns.class_('CANSensor', sensor.Sensor, cg.Component)

CONF_BIND_TO = "bind_to"
DOMAINS = ["switch", "binary_sensor", "sensor"]
IS_GATEWAY_COMPILATION = False

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(CANHub),
    cv.Required(CONF_PIN): pins.internal_gpio_pin_number,
    cv.Optional("bitrate", default=25000): cv.int_,
}).extend(cv.COMPONENT_SCHEMA)

def get_network_map(target_gateway_id):
    net_map = {}
    current_dir = os.getcwd()
    for file in os.listdir(current_dir):
        if file.endswith(".yaml") or file.endswith(".yml"):
            try:
                content = yaml_util.load_yaml(os.path.join(current_dir, file))
                if not content or "esphome" not in content: continue
                node_name = content["esphome"]["name"]
                match = re.search(r'(\d+)-(\d+)$', node_name)
                if match and int(match.group(1)) == target_gateway_id:
                    p_id = int(match.group(2))
                    peref_key = f"{target_gateway_id}-{p_id}"
                    net_map[peref_key] = {d: [] for d in DOMAINS}
                    for d in DOMAINS:
                        if d in content:
                            items = content[d]
                            if isinstance(items, dict): items = [items]
                            for item in items:
                                if item.get("platform") == "can":
                                    net_map[peref_key][d].append((str(item.get("id")), item.get("name")))
            except Exception: continue
    return net_map

def find_source_by_component_id(target_id):
    current_dir = os.getcwd()
    domain_types = {"switch": 0x01, "binary_sensor": 0x02, "sensor": 0x03}
    for file in os.listdir(current_dir):
        if file.endswith(".yaml") or file.endswith(".yml"):
            try:
                content = yaml_util.load_yaml(os.path.join(current_dir, file))
                if not content or "esphome" not in content: continue
                match = re.search(r'(\d+)-(\d+)$', content["esphome"]["name"])
                if not match: continue
                g_id, p_id = map(int, match.groups())
                for d, type_code in domain_types.items():
                    if d in content:
                        items = content[d]
                        if isinstance(items, dict): items = [items]
                        for idx, item in enumerate(items):
                            if str(item.get("id")) == target_id:
                                return (0x210 + p_id), idx, type_code
            except Exception: continue
    raise cv.Invalid(f"Компонент с id '{target_id}' для P2P связи не найден!")

async def to_code(config):
    global IS_GATEWAY_COMPILATION
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    node_name = cg.App.get_name()
    gateway_match = re.search(r'-(\d+)$', node_name)
    if gateway_match and "peref" not in node_name:
        IS_GATEWAY_COMPILATION = True
        gateway_id = int(gateway_match.group(1))
        await build_gateway_entities(var, get_network_map(gateway_id), gateway_id)
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_bitrate(config["bitrate"]))

async def build_gateway_entities(parent, net_map, gateway_id):
    bindings = {"switch": (CANSwitch, switch.register_switch, 0x01),
                "binary_sensor": (CANBinarySensor, binary_sensor.register_binary_sensor, 0x02),
                "sensor": (CANSensor, sensor.register_sensor, 0x03)}
    for peref_str, domains in net_map.items():
        _, p_id = map(int, peref_str.split('-'))
        cmd_id, stat_id = (0x110 + p_id), (0x210 + p_id)
        for d, (cpp_class, reg_fn, d_type) in bindings.items():
            for idx, (comp_id, comp_name) in enumerate(domains[d]):
                final_name = comp_name if comp_name else comp_id
                ha_entity_id = f"peref{gateway_id}_{p_id}_{comp_id}"
                svar = cg.new_Pvariable(cg.ID(ha_entity_id, is_declaration=True, type=cpp_class))
                fake_config = {
                    CONF_NAME: f"peref{gateway_id}-{p_id} {final_name}", 
                    CONF_ID: cg.ID(ha_entity_id, is_declaration=False, type=cpp_class)
                }
                await cg.register_component(svar, fake_config)
                await reg_fn(svar, fake_config)
                cg.add(svar.set_parent(parent))
                cg.add(svar.set_can_ids(cmd_id, stat_id))
                cg.add(svar.set_meta(idx, d_type))

def setup_peripheral_platform_template(config, cpp_class, register_fn, domain_type, counter_attr):
    from . import can_ns, CANHub, IS_GATEWAY_COMPILATION, CONF_BIND_TO, find_source_by_component_id
    if IS_GATEWAY_COMPILATION: return
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(cg.App.register_component(var))
    register_fn(var, config)
    parent = cg.use_id(CANHub)(config["can_hub_id"])
    cg.add(var.set_parent(parent))
    idx = getattr(parent, counter_attr, 0)
    cg.add(var.set_meta(idx, domain_type))
    setattr(parent, counter_attr, idx + 1)
    if CONF_BIND_TO in config:
        src_can_id, src_idx, src_type = find_source_by_component_id(config[CONF_BIND_TO])
        cg.add(var.set_listen_source(src_can_id, src_idx, src_type))