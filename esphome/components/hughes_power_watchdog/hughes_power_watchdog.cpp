#include "hughes_power_watchdog.h"

#ifdef USE_ESP32

namespace esphome {
namespace hughes_power_watchdog {

static const char *const TAG = "hughes_power_watchdog";

/** Define the error codes and help string.  Found by looking at app **/

static const std::string Error_00 = "All Good";  // Not error.

static const std::string Error_01 =
    "Your Watchdog has determined that the Line 1 voltage coming from the park has either\
  exceeded 132 or dropped below 104 and shut off the park power.  Both voltages are damaging to your RV. The Watchdog\
  will now constantly check the voltage, once the voltage stays in the safe range for 90 seconds, the Watchdog will\
  turn the power back on.";

static const std::string Error_02 =
    "Your Watchdog has determined that the Line 2 voltage coming from the park has either\
  exceeded 132 or dropped below 104 and shut off the park power.  Both voltages are damaging to your RV. The Watchdog\
  will now constantly check the voltage, once the voltage stays in the safe range for 90 seconds, the Watchdog will\
  turn the power back on.";

static const std::string Error_03 = "Your Watchdog has determined that you have exceeded your Line 1 amp rating.\
  Two issues; One, the park circuit should have tripped and apparently didn't. Two, you are drawing more amps than\
  your RV can handle.  At this point go into the RV and turn off major appliances such as an air conditioner or\
  microwave.  You will now draw fewer amps. Check the Line 1 voltage as low voltage causes higher amperage.";

static const std::string Error_04 = "Your Watchdog has determined that you have exceeded your Line 2 amp rating.\
  Two issues; One, the park circuit should have tripped and apparently didn't. Two, you are drawing more amps than\
  your RV can handle.  At this point go into the RV and turn off major appliances such as an air conditioner or\
  microwave.  You will now draw fewer amps. Check the Line 1 voltage as low voltage causes higher amperage.";

static const std::string Error_05 = "Your Power Watchdog has noticed a very serious condition.  The park power\
  has the Line 1 Hot and Neutral wire reversed somewhere. This could cause severe damage to your appliances.\
  See park management and tell them they have a neutral and hot wire reversed. The Watchdog will not allow park\
  power through in this condition.  Once this condition is fixed the Watchdog will automatically turn on the power\
  after a ninety second delay.";

static const std::string Error_06 = "Your Power Watchdog has noticed a very serious condition.  The park power\
  has the Line 2 Hot and Neutral wire reversed somewhere. This could cause severe damage to your appliances.\
  See park management and tell them they have a neutral and hot wire reversed. The Watchdog will not allow park\
  power through in this condition.  Once this condition is fixed the Watchdog will automatically turn on the power\
  after a ninety second delay.";

static const std::string Error_07 = "Your Power Watchdog has found a condition that can be very serious.  You have\
  lost your ground connection. If an electrical wire touches metal in your coach it normally goes to ground and that\
  triggers the breaker, currently this safety feature is not working.  You could receive a serious shock. Contact park\
  management. The Watchdog will not allow park power to the RV without a ground connection. The Watchdog will now\
  monitor the ground wire. Once this condition is fixed the Watchdog will wait 90 seconds and turn the power back on.";

static const std::string Error_08 = "The Power Watchdog is reporting you have no neutral circuit inside your RV.\
  The neutral wire provides a return path for electricity.  Without a neutral, you will begin to burn up your\
  appliances.  You can very quickly have major damage. The Power Watchdog will not allow power to an RV that does\
  not have a proper neutral connection.  You must service your RV and restore the neutral circuit inside your RV.\
  Once this condition is fixed, you need to push neutral reset button and Watchdog will turn back on as normal.";

static const std::string Error_09 = "The Power Watchdog is sensing the surge absorption capacity has been used up.\
  This is your only warning, the RV will operate with a used up surge board.  Each surge protector can only take so\
  many surges before they're done.  Fortunately, the Watchdog's surge absorption board is replaceable. Go to\
  hughesautoformers.com and order a new board.";

// Array of error strings indexed by error code value
static const std::string ErrorText[] = {Error_00, Error_01, Error_02, Error_03, Error_04,
                                        Error_05, Error_06, Error_07, Error_08, Error_09};

// Macro to convert 4 bytes of big endian data into int
#define ReadBigEndianInt32(data, offset) \
  ((data[offset + 3] << 0) | (data[offset + 2] << 8) | (data[offset + 1] << 16) | (data[offset + 0] << 24))

#define IsFirstHalfOfMessage(data) ((data[0] == 1) && (data[1] == 3) && (data[2] == 0x20))

#define IsLine1(data) ((data[37] == 0) && (data[38] == 0) && (data[39] == 0))

#define IsLine2(data) ((data[37] == 1) && (data[38] == 1) && (data[39] == 1))

HughesPowerWatchdog::HughesPowerWatchdog()
    : PollingComponent(5000),
      service_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID)),
      char_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(CHARACTERISTIC_UUID_TX)) {
  this->chunk_1_content_populated = 0;
  this->line1_v_ = 0.0f;
  this->line2_v_ = 0.0f;
  this->line1_c_ = 0.0f;
  this->line2_c_ = 0.0f;
  this->line1_p_ = 0.0f;
  this->line2_p_ = 0.0f;
  this->line1_ce_ = 0.0f;
  this->line2_ce_ = 0.0f;
  this->error_code_value_ = 0;
  this->new_data_ = false;
}

void HughesPowerWatchdog::dump_config() {
  ESP_LOGCONFIG(TAG, "Hughes Power Watchdog");
  LOG_SENSOR("  ", "Voltage Line 1", this->voltage_l1_);
  LOG_SENSOR("  ", "Current Line 1", this->current_l1_);
  LOG_SENSOR("  ", "Power Line 1", this->power_l1_);
  LOG_SENSOR("  ", "Voltage Line 2", this->voltage_l2_);
  LOG_SENSOR("  ", "Current Line 2", this->current_l2_);
  LOG_SENSOR("  ", "Power Line 2", this->power_l2_);
  LOG_SENSOR("  ", "Power Combined", this->power_combined_);
  LOG_SENSOR("  ", "Cumulative Energy", this->cumulative_energy_);
  LOG_SENSOR("  ", "Error Code Value", this->error_code_);
  LOG_TEXT_SENSOR("  ", "Error Text String", this->error_text_);
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
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle = 0;
      auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        this->status_set_warning();
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
                 this->char_uuid_.to_string().c_str());
        break;
      }
      this->handle = chr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                      chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->get_conn_id() || param->notify.handle != this->handle)
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

/**
 * @brief Internal function to decode the BLE data and extract the fields of interest.
 * Watchdog sends two 20 byte chunks that once combined makes up a data message.
 *
 * For 50A (two hot lines) each line has its own message.
 * For 30A (one hot line) there should only be one message and it should indicate line 1.
 *
 * @param value - ble data (array of bytes)
 * @param value_len - length of ble data
 */
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
           \______/ \_________/ \_________/ \_________/ \_________/ \_/
            header     volts       amps        watts       energy    error_code

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

  this->error_code_value_ = this->msg_buffer_[19];
  if (line == 1) {
    this->line1_v_ = volts;
    this->line1_c_ = amps;
    this->line1_p_ = watts;
    this->line1_ce_ = energy;
  } else {
    this->line2_v_ = volts;
    this->line2_c_ = amps;
    this->line2_p_ = watts;
    this->line2_ce_ = energy;
  }
  this->new_data_ = true;
}

/**
 * @brief Internal function to update the state of
 * all valid sensors.  Parameter controls if
 * real data will be used or if NAN should be reported
 * so that state shows up as unavailable.
 *
 * @param UseInstanceData  true: use data
 *                         false: use NAN
 */
void HughesPowerWatchdog::ReportSensor(bool UseInstanceData) {
  if (UseInstanceData) {
    // Voltage (volts)
    if (this->voltage_l1_ != nullptr) {
      this->voltage_l1_->publish_state(this->line1_v_);
    }
    if (this->voltage_l2_ != nullptr) {
      this->voltage_l2_->publish_state(this->line2_v_);
    }

    // Current (amps)
    if (this->current_l1_ != nullptr) {
      this->current_l1_->publish_state(this->line1_c_);
    }
    if (this->current_l2_ != nullptr) {
      this->current_l2_->publish_state(this->line2_c_);
    }

    // Power (watts)
    if (this->power_l1_ != nullptr) {
      this->power_l1_->publish_state(this->line1_p_);
    }
    if (this->power_l2_ != nullptr) {
      this->power_l2_->publish_state(this->line2_p_);
    }
    if (this->power_combined_ != nullptr) {
      this->power_combined_->publish_state(this->line2_p_ + this->line1_p_);
    }

    // Cumulative Power since user reset (KilowattHours)
    if (this->cumulative_energy_ != nullptr) {
      this->cumulative_energy_->publish_state(this->line1_ce_ + this->line2_ce_);
    }

    // Error codes and help text
    if (this->error_code_ != nullptr) {
      this->error_code_->publish_state(this->error_code_value_);
    }
    if (this->error_text_ != nullptr) {
      this->error_text_->publish_state(ErrorText[this->error_code_value_]);
    }
  } else {
    if (this->voltage_l1_ != nullptr) {
      this->voltage_l1_->publish_state(NAN);
    }
    if (this->voltage_l2_ != nullptr) {
      this->voltage_l2_->publish_state(NAN);
    }

    // Current (amps)
    if (this->current_l1_ != nullptr) {
      this->current_l1_->publish_state(NAN);
    }
    if (this->current_l2_ != nullptr) {
      this->current_l2_->publish_state(NAN);
    }

    // Power (watts)
    if (this->power_l1_ != nullptr) {
      this->power_l1_->publish_state(NAN);
    }
    if (this->power_l2_ != nullptr) {
      this->power_l2_->publish_state(NAN);
    }
    if (this->power_combined_ != nullptr) {
      this->power_combined_->publish_state(NAN);
    }

    // Cumulative Power since user reset (KilowattHours)
    if (this->cumulative_energy_ != nullptr) {
      this->cumulative_energy_->publish_state(NAN);
    }

    // Error codes and help text
    if (this->error_code_ != nullptr) {
      this->error_code_->publish_state(NAN);
    }
    if (this->error_text_ != nullptr) {
      this->error_text_->publish_state("");
    }
  }
}

/**
 * @brief PolledComponents have an update function
 * that is called at each polling period.  If new data exists
 * and the sensor is connected then report data.
 *
 */
void HughesPowerWatchdog::update() {
  ESP_LOGV(TAG, "Update Called");

  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    ESP_LOGV(TAG, "Not Established");
    this->ReportSensor(false);
  } else if (this->new_data_ == false) {
    ESP_LOGV(TAG, "No New Data");
    // Just don't report this update.
  } else {
    this->ReportSensor(true);
    this->new_data_ = false;
  }
}

}  // namespace hughes_power_watchdog
}  // namespace esphome

#endif
