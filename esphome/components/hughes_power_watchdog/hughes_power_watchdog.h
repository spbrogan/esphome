#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hughes_power_watchdog {

static const char *const SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";            // vendor specific
static const char *const CHARACTERISTIC_UUID_TX = "0000ffe2-0000-1000-8000-00805f9b34fb";  // TX
static const char *const CHARACTERISTIC_UUID_RX = "0000fff5-0000-1000-8000-00805f9b34fb";  // RX

class HughesPowerWatchdog : public PollingComponent, public ble_client::BLEClientNode {
 public:
  HughesPowerWatchdog();
  void dump_config() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_voltage_line_1(sensor::Sensor *voltage) { voltage_l1_ = voltage; };  // volts
  void set_voltage_line_2(sensor::Sensor *voltage) { voltage_l2_ = voltage; };  // volts

  void set_current_line_1(sensor::Sensor *current) { current_l1_ = current; };  // amps
  void set_current_line_2(sensor::Sensor *current) { current_l2_ = current; };  // amps

  void set_power_line_1(sensor::Sensor *power) { power_l1_ = power; };          // watts
  void set_power_line_2(sensor::Sensor *power) { power_l2_ = power; };          // watts
  void set_power_combined(sensor::Sensor *power) { power_combined_ = power; };  // watts

  void set_cumulative_energy(sensor::Sensor *energy) { cumulative_energy_ = energy; };  // kilowatt hours

  void set_error_code(sensor::Sensor *error) { error_code_ = error; };  // error code between 0 - 9
  void set_error_text(text_sensor::TextSensor *error_text) { error_text_ = error_text; };  // error message text
  uint16_t handle;

 protected:
  void process_tx_notification(uint8_t *value, uint16_t value_len);
  sensor::Sensor *voltage_l1_{nullptr};
  sensor::Sensor *voltage_l2_{nullptr};
  sensor::Sensor *current_l1_{nullptr};
  sensor::Sensor *current_l2_{nullptr};
  sensor::Sensor *power_l1_{nullptr};
  sensor::Sensor *power_l2_{nullptr};
  sensor::Sensor *power_combined_{nullptr};
  sensor::Sensor *cumulative_energy_{nullptr};
  sensor::Sensor *error_code_{nullptr};
  text_sensor::TextSensor *error_text_{nullptr};
  void ReportSensor(bool UseInstanceData);

  uint8_t chunk_1_content_populated;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID char_uuid_;

  uint8_t msg_buffer_[40];
  float line1_v_;             // line 1 volts
  float line2_v_;             // line 2 volts
  float line1_c_;             // line 1 current
  float line2_c_;             // line 2 current
  float line1_p_;             // line 1 power
  float line2_p_;             // line 2 power
  float line1_ce_;            // line 1 cumulative energy
  float line2_ce_;            // line 2 cumulative energy
  uint8_t error_code_value_;  // value of
  bool new_data_;             // boolean to indicate new data
};

}  // namespace hughes_power_watchdog
}  // namespace esphome

#endif
