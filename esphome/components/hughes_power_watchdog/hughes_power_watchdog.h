#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hughes_power_watchdog {

static const char *const SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";         //vendor specific
static const char *const CHARACTERISTIC_UUID_TX = "0000ffe2-0000-1000-8000-00805f9b34fb";  //TX
static const char *const CHARACTERISTIC_UUID_RX = "0000fff5-0000-1000-8000-00805f9b34fb";  //RX




class HughesPowerWatchdog : public Component, public ble_client::BLEClientNode {

 public:
  HughesPowerWatchdog();
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;


  void set_voltage_line_1(sensor::Sensor *voltage) { voltage_l1_ = voltage; };  //volts
  void set_voltage_line_2(sensor::Sensor *voltage) { voltage_l2_ = voltage; };  //volts

  void set_current_line_1(sensor::Sensor *current) { current_l1_ = current; };  //amps
  void set_current_line_2(sensor::Sensor *current) { current_l2_ = current; };  //amps

  void set_power_line_1(sensor::Sensor *power) { power_l1_ = power; };  //watts
  void set_power_line_2(sensor::Sensor *power) { power_l2_ = power; };  //watts

  void set_cumulative_energy(sensor::Sensor *energy) { cumulative_energy_ = energy; };  //watts
  void set_line_count(uint8_t lines) { supported_input_lines_ = lines; }
  uint16_t handle;
 protected:
  void process_tx_notification(uint8_t *value, uint16_t value_len);
  sensor::Sensor *voltage_l1_{nullptr};
  sensor::Sensor *voltage_l2_{nullptr};
  sensor::Sensor *current_l1_{nullptr};
  sensor::Sensor *current_l2_{nullptr};
  sensor::Sensor *power_l1_{nullptr};
  sensor::Sensor *power_l2_{nullptr};
  sensor::Sensor *cumulative_energy_{nullptr};

  uint8_t supported_input_lines_; // 2 lines for 50A product, 1 line for 30A product


  
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID char_uuid_;

  uint8_t msg_buffer_[40];
};

}  // namespace mopeka_pro_check
}  // namespace esphome

#endif
