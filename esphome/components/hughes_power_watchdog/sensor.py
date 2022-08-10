import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client
from esphome.const import (
    CONF_ID,
    CONF_VOLTAGE,
    CONF_CURRENT,
    CONF_POWER,
    CONF_TOTAL_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    ICON_POWER,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT
)
_CONF_LINE_1 = "_line_1"
_CONF_LINE_2 = "_line_2"

CONF_VOLTAGE_LINE_1 = CONF_VOLTAGE + _CONF_LINE_1
CONF_CURRENT_LINE_1 = CONF_CURRENT + _CONF_LINE_1
CONF_POWER_LINE_1 = CONF_POWER + _CONF_LINE_1

CONF_VOLTAGE_LINE_2 = CONF_VOLTAGE + _CONF_LINE_2
CONF_CURRENT_LINE_2 = CONF_CURRENT + _CONF_LINE_2
CONF_POWER_LINE_2 = CONF_POWER + _CONF_LINE_2


ICON_VOLTAGE = "mdi:flash-triangle"
ICON_TOTAL_POWER = "mdi:sine-wave"


CODEOWNERS = ["@spbrogan"]
DEPENDENCIES = ["ble_client"]

hughes_power_watchdog_ns = cg.esphome_ns.namespace("hughes_power_watchdog")
HughesPowerWatchdog = hughes_power_watchdog_ns.class_(
    "HughesPowerWatchdog", ble_client.BLEClientNode, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HughesPowerWatchdog),
            cv.Optional(CONF_VOLTAGE_LINE_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_VOLTAGE,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT_LINE_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_LINE_1): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLTAGE_LINE_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                icon=ICON_VOLTAGE,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT_LINE_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_LINE_2): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TOTAL_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                icon=ICON_TOTAL_POWER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("5sec"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if CONF_VOLTAGE_LINE_1 in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE_LINE_1])
        cg.add(var.set_voltage_line_1(sens))

    if CONF_CURRENT_LINE_1 in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_LINE_1])
        cg.add(var.set_current_line_1(sens))

    if CONF_POWER_LINE_1 in config:
        sens = await sensor.new_sensor(config[CONF_POWER_LINE_1])
        cg.add(var.set_power_line_1(sens))

    if CONF_VOLTAGE_LINE_2 in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE_LINE_2])
        cg.add(var.set_voltage_line_2(sens))

    if CONF_CURRENT_LINE_2 in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_LINE_2])
        cg.add(var.set_current_line_2(sens))

    if CONF_POWER_LINE_2 in config:
        sens = await sensor.new_sensor(config[CONF_POWER_LINE_2])
        cg.add(var.set_power_line_2(sens))

    if CONF_TOTAL_POWER in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_POWER])
        cg.add(var.set_cumulative_energy(sens))
