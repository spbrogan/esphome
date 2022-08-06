#include "hughes_power_watchdog.h"

#ifdef USE_ESP32

namespace esphome {
namespace hughes_power_watchdog {

static const char *const TAG = "hughes_power_watchdog";

#define ReadBigEndianInt32(data, offset) \
  ((data[offset + 3] << 0) | (data[offset + 2] << 8) | (data[offset + 1] << 16) | (data[offset + 0] << 24))

#define IsFirstHalfOfMessage(data) ((data[0] == 1) && (data[1] == 3) && (data[2] == 0x20))

#define IsLine1(data) ((data[37] == 0) && (data[38] == 0) && (data[39] == 0))

#define IsLine2(data) ((data[37] == 1) && (data[38] == 1) && (data[39] == 1))

HughesPowerWatchdog::HughesPowerWatchdog()
    : service_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID)),
      char_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(CHARACTERISTIC_UUID_TX)) {
  this->chunk_1_content_populated = 0;
}

void HughesPowerWatchdog::dump_config() {
  ESP_LOGCONFIG(TAG, "Hughes Power Watchdog");
  LOG_SENSOR("  ", "Voltage Line 1", this->voltage_l1_);
  LOG_SENSOR("  ", "Current Line 1", this->current_l1_);
  LOG_SENSOR("  ", "Power Line 1", this->power_l1_);
  LOG_SENSOR("  ", "Voltage Line 2", this->voltage_l2_);
  LOG_SENSOR("  ", "Current Line 2", this->current_l2_);
  LOG_SENSOR("  ", "Power Line 2", this->power_l2_);
  LOG_SENSOR("  ", "Cumulative Energy", this->cumulative_energy_);
}

void HughesPowerWatchdog::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected successfully!");
        break;
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected!");
      this->status_set_warning();
      // TODO: Should we clear the state of the sensors.
      //       Depends on if we are going to stay connected all the time
      //       or if we should connect , then handle a few, disconnect , sleep 10 seconds
      //       and do it over.  Since none of these are expected to be battery powered
      //       lets keep connected for now.

      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle = 0;
      auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        this->status_set_warning();
        // mark sensors unavailable?
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
                 this->char_uuid_.to_string().c_str());
        break;
      }
      this->handle = chr->handle;

      auto status =
          esp_ble_gattc_register_for_notify(this->parent()->gattc_if, this->parent()->remote_bda, chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->conn_id || param->notify.handle != this->handle)
        break;
      ESP_LOGV(TAG, "ESP_GATTC_NOTIFY_EVT: handle=0x%x, data length=%d", param->notify.handle, param->notify.value_len);
      this->process_tx_notification(param->notify.value, param->notify.value_len);
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      break;
    }
    default:
      break;
  }
}

void HughesPowerWatchdog::process_tx_notification(uint8_t *value, uint16_t value_len) {
  if (value_len != 20) {
    ESP_LOGW(TAG, "process_tx_notification - unsupported value length (%d)", value_len);

    ESP_LOGV(TAG, "UNKNOWN TX BLE DATA:");
    for (int i = 0; i < value_len; i++) {
      ESP_LOGV(TAG, "0x%02x", *(value + i));
    }
    return;
  }

  // Log in single line since we know it is 20 bytes long
  ESP_LOGV(TAG, "TX BLE DATA: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \
0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
           *(value), *(value + 1), *(value + 2), *(value + 3), *(value + 4), *(value + 5), *(value + 6), *(value + 7),
           *(value + 8), *(value + 9), *(value + 10), *(value + 11), *(value + 12), *(value + 13), *(value + 14),
           *(value + 15), *(value + 16), *(value + 17), *(value + 18), *(value + 19));

  /** 0 => 01 03 20 00 12 5b a4 00 01 43 07 00 93 c8 08 00 6a cd 68 00
           \______/ \_________/ \_________/ \_________/ \_________/
            header     volts       amps        watts       energy

      1 => 00 03 d0 00 53 d7 01 ff df 10 00 00 00 17 6d c7 1c 00 00 00
                                                              \______/
                                                                line
  **/
  if (IsFirstHalfOfMessage(value)) {
    memcpy(this->msg_buffer_, value, value_len);
    this->chunk_1_content_populated = 1;
    return;
  }

  // catch case where buffer didn't get chunk 1...ignore chunk 2
  if (this->chunk_1_content_populated != 1) {
    ESP_LOGW(TAG, "TX Msg chunk received out of order.  Ignoring chunk.");
    return;
  }

  this->chunk_1_content_populated = 0;  // reset chunk counter
  // Second half - now we can process buffer and update sensors
  memcpy(&this->msg_buffer_[20], value, value_len);

  // Figure out if line 1 or line 2
  uint8_t line = 0;
  if (IsLine1(this->msg_buffer_)) {
    line = 1;
  } else if (IsLine2(this->msg_buffer_)) {
    line = 2;
  } else {
    ESP_LOGE(TAG, "process_tx_notification - unsupported line value (0x%X 0x%X 0x%X)", this->msg_buffer_[37],
             this->msg_buffer_[38], this->msg_buffer_[39]);
    return;
  }

  float volts = (float) ReadBigEndianInt32(this->msg_buffer_, 3) / 10000;
  float amps = (float) ReadBigEndianInt32(this->msg_buffer_, 7) / 10000;
  float watts = (float) ReadBigEndianInt32(this->msg_buffer_, 11) / 10000;
  float energy = (float) ReadBigEndianInt32(this->msg_buffer_, 15) / 10000;

  if (line == 1) {
    if (this->voltage_l1_ != nullptr) {
      this->voltage_l1_->publish_state(volts);
    }

    if (this->current_l1_ != nullptr) {
      this->current_l1_->publish_state(amps);
    }

    if (this->power_l1_ != nullptr) {
      this->power_l1_->publish_state(watts);
    }

    /** ONLY VALID FOR LINE 2
    if (this->cumulative_energy_ != nullptr) {
      this->cumulative_energy_->publish_state(energy);
    }
    **/
  } else {
    if (this->voltage_l2_ != nullptr) {
      this->voltage_l2_->publish_state(volts);
    }

    if (this->current_l2_ != nullptr) {
      this->current_l2_->publish_state(amps);
    }

    if (this->power_l2_ != nullptr) {
      this->power_l2_->publish_state(watts);
    }

    if (this->cumulative_energy_ != nullptr) {
      this->cumulative_energy_->publish_state(energy);
    }
  }
}

}  // namespace hughes_power_watchdog
}  // namespace esphome

#endif
