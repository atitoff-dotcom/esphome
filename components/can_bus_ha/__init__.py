import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, yaml_util
from esphome.components import switch, binary_sensor, sensor, number
from esphome.const import CONF_ID, CONF_PIN, CONF_NAME
import os
import re

can_ns = cg.esphome_ns.namespace('can_bus_ha')
CANHub = can_ns.class_('CANHub', cg.Component)

CANSwitch = can_ns.class_('CANSwitch', switch.Switch, cg.Component)
CANBinarySensor = can_ns.class_('CANBinarySensor', binary_sensor.BinarySensor, cg.Component)
CANSensor = can_ns.class_('CANSensor', sensor.Sensor, cg.Component)
CANNumber = can_ns.class_('CANNumber', number.Number, cg.Component)

CONF_BIND_TO = "bind_to"
DOMAINS = ["switch", "binary_sensor", "sensor", "number"]
IS_GATEWAY_COMPILATION = False
AUTO_LOAD = ["switch", "binary_sensor", "sensor", "number", "text_sensor"]

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(CANHub),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional("bitrate", default=25000): cv.int_,
    cv.Optional("gateway_id"): cv.int_,
    cv.Optional("peripheral_id"): cv.int_,
}).extend(cv.COMPONENT_SCHEMA)

def get_network_map(target_gateway_id):
    net_map = {}
    current_dir = os.getcwd()
    for file in os.listdir(current_dir):
        if file.endswith(".yaml") or file.endswith(".yml"):
            try:
                content = yaml_util.load_yaml(os.path.join(current_dir, file))
                if not content or "esphome" not in content: continue
                if "can_bus_ha" not in content: continue
                can_cfg = content["can_bus_ha"]
                g_id = can_cfg.get("gateway_id")
                p_id = can_cfg.get("peripheral_id")
                if g_id == target_gateway_id and p_id is not None:
                    peref_key = f"{target_gateway_id}-{p_id}"
                    net_map[peref_key] = {d: [] for d in DOMAINS}
                    for d in DOMAINS:
                        if d in content:
                            items = content[d]
                            if isinstance(items, dict): items = [items]
                            for item in items:
                                if item.get("platform") == "can_bus_ha":
                                    net_map[peref_key][d].append({
                                        "id": str(item.get("id")),
                                        "name": item.get("name"),
                                        "min_value": item.get("min_value", 0.0),
                                        "max_value": item.get("max_value", 100.0),
                                        "step": item.get("step", 1.0),
                                    })
            except Exception: continue
    return net_map

def find_source_by_component_id(target_id):
    current_dir = os.getcwd()
    domain_types = {"switch": 0x01, "binary_sensor": 0x02, "sensor": 0x03, "number": 0x04}
    for file in os.listdir(current_dir):
        if file.endswith(".yaml") or file.endswith(".yml"):
            try:
                content = yaml_util.load_yaml(os.path.join(current_dir, file))
                if not content or "esphome" not in content: continue
                if "can_bus_ha" not in content: continue
                can_cfg = content["can_bus_ha"]
                p_id = can_cfg.get("peripheral_id")
                if p_id is None: continue
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
    
    if "gateway_id" in config:
        cg.add(var.set_gateway_id(config["gateway_id"]))
        if "peripheral_id" not in config:
            IS_GATEWAY_COMPILATION = True
            gateway_id = config["gateway_id"]
            await build_gateway_entities(var, get_network_map(gateway_id), gateway_id)
            
    if "peripheral_id" in config:
        cg.add(var.set_peripheral_id(config["peripheral_id"]))
        
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_bitrate(config["bitrate"]))

async def build_gateway_entities(parent, net_map, gateway_id):
    async def reg_switch(svar, fake_config):
        await switch.register_switch(svar, fake_config)
    async def reg_binary_sensor(svar, fake_config):
        await binary_sensor.register_binary_sensor(svar, fake_config)
    async def reg_sensor(svar, fake_config):
        await sensor.register_sensor(svar, fake_config)
    async def reg_number(svar, fake_config):
        await number.register_number(
            svar, fake_config,
            min_value=fake_config["min_value"],
            max_value=fake_config["max_value"],
            step=fake_config["step"]
        )

    bindings = {"switch": (CANSwitch, reg_switch, 0x01),
                "binary_sensor": (CANBinarySensor, reg_binary_sensor, 0x02),
                "sensor": (CANSensor, reg_sensor, 0x03),
                "number": (CANNumber, reg_number, 0x04)}
    for peref_str, domains in net_map.items():
        _, p_id = map(int, peref_str.split('-'))
        cmd_id, stat_id = (0x110 + p_id), (0x210 + p_id)
        for d, (cpp_class, reg_fn, d_type) in bindings.items():
            for idx, ent_cfg in enumerate(domains[d]):
                comp_id = ent_cfg["id"]
                comp_name = ent_cfg["name"]
                final_name = comp_name if comp_name else comp_id
                ha_entity_id = f"peref{gateway_id}_{p_id}_{comp_id}"
                svar = cg.new_Pvariable(cg.ID(ha_entity_id, is_declaration=True, type=cpp_class))
                fake_config = {
                    CONF_NAME: f"peref{gateway_id}-{p_id} {final_name}", 
                    CONF_ID: cg.ID(ha_entity_id, is_declaration=False, type=cpp_class),
                    "min_value": ent_cfg.get("min_value"),
                    "max_value": ent_cfg.get("max_value"),
                    "step": ent_cfg.get("step"),
                }
                await cg.register_component(svar, fake_config)
                await reg_fn(svar, fake_config)
                cg.add(svar.set_parent(parent))
                cg.add(svar.set_can_ids(cmd_id, stat_id))
                cg.add(svar.set_meta(idx, d_type))

        # Регистрируем виртуальный переключатель WiFi для прошивки через OTA
        ha_wifi_id = f"peref{gateway_id}_{p_id}_wifi"
        svar = cg.new_Pvariable(cg.ID(ha_wifi_id, is_declaration=True, type=CANSwitch))
        fake_wifi_config = {
            CONF_NAME: f"peref{gateway_id}-{p_id} WiFi",
            CONF_ID: cg.ID(ha_wifi_id, is_declaration=False, type=CANSwitch)
        }
        await cg.register_component(svar, fake_wifi_config)
        await switch.register_switch(svar, fake_wifi_config)
        cg.add(svar.set_parent(parent))
        cg.add(svar.set_can_ids(cmd_id, stat_id))
        cg.add(svar.set_meta(0xFF, 0xFF))

    # Регистрируем текстовый статус прошивки на шлюзе
    from esphome.components import text_sensor
    status_sensor_id = f"peref{gateway_id}_wifi_status"
    status_config = {
        CONF_ID: cg.ID(status_sensor_id, is_declaration=False, type=text_sensor.TextSensor),
        CONF_NAME: f"peref{gateway_id} WiFi Status",
    }
    svar_status = cg.new_Pvariable(status_config[CONF_ID])
    await cg.register_component(svar_status, status_config)
    await text_sensor.register_text_sensor(svar_status, status_config)
    cg.add(parent.set_wifi_status_sensor(svar_status))


HUB_COUNTERS = {}

def get_hub_counter(hub_id, counter_attr):
    if hub_id not in HUB_COUNTERS:
        HUB_COUNTERS[hub_id] = {}
    return HUB_COUNTERS[hub_id].get(counter_attr, 0)

def increment_hub_counter(hub_id, counter_attr):
    if hub_id not in HUB_COUNTERS:
        HUB_COUNTERS[hub_id] = {}
    HUB_COUNTERS[hub_id][counter_attr] = HUB_COUNTERS[hub_id].get(counter_attr, 0) + 1

async def setup_peripheral_platform_template(config, cpp_class, register_fn, domain_type, counter_attr):
    from . import can_ns, CANHub, IS_GATEWAY_COMPILATION, CONF_BIND_TO, find_source_by_component_id
    if IS_GATEWAY_COMPILATION: return None
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_fn(var, config)
    parent = await cg.get_variable(config["can_hub_id"])
    cg.add(var.set_parent(parent))
    hub_id = str(config["can_hub_id"])
    idx = get_hub_counter(hub_id, counter_attr)
    cg.add(var.set_meta(idx, domain_type))
    increment_hub_counter(hub_id, counter_attr)
    if CONF_BIND_TO in config:
        src_can_id, src_idx, src_type = find_source_by_component_id(config[CONF_BIND_TO])
        cg.add(var.set_listen_source(src_can_id, src_idx, src_type))
    return var