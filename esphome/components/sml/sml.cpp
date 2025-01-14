#include "sml.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "sml_parser.h"

namespace esphome {
namespace sml {

static const char *const TAG = "sml";

const char START_BYTES_DETECTED = 1;
const char END_BYTES_DETECTED = 2;

SmlListener::SmlListener(std::string server_id, std::string obis_code)
    : server_id(std::move(server_id)), obis_code(std::move(obis_code)) {}

char Sml::check_start_end_bytes_(uint8_t byte) {
  this->incoming_mask_ = (this->incoming_mask_ << 2) | get_code(byte);

  if (this->incoming_mask_ == START_MASK)
    return START_BYTES_DETECTED;
  if ((this->incoming_mask_ >> 6) == END_MASK)
    return END_BYTES_DETECTED;
  return 0;
}

void Sml::loop() {
  while (available()) {
    const char c = read();

    if (this->record_)
      this->sml_data_.emplace_back(c);

    switch (this->check_start_end_bytes_(c)) {
      case START_BYTES_DETECTED: {
        this->record_ = true;
        this->sml_data_.clear();
        // add start sequence (for callbacks)
        this->sml_data_.insert(this->sml_data_.begin(), START_SEQ.begin(), START_SEQ.end());
        break;
      };
      case END_BYTES_DETECTED: {
        if (this->record_) {
          this->record_ = false;

          bool valid = check_sml_data(this->sml_data_);

          // call callbacks
          this->data_callbacks_.call(this->sml_data_, valid);

          if (!valid)
            break;

          // discard start/end sequence
          auto file_begin = this->sml_data_.begin() + START_SEQ.size();
          auto file_length = this->sml_data_.size() - START_SEQ.size() - 8;
          this->process_sml_file_(byte_span(&*file_begin, file_length));
        }
        break;
      };
    };
  }
}

void Sml::add_on_data_callback(std::function<void(std::vector<uint8_t>, bool)> &&callback) {
  this->data_callbacks_.add(std::move(callback));
}

void Sml::process_sml_file_(const byte_span &sml_data) {
  SmlFile(sml_data).for_each_obis_info([this](const ObisInfo &obis_info) { this->publish_value_(obis_info); });
}

void Sml::publish_value_(const ObisInfo &obis_info) {
  ESP_LOGD(TAG, "OBIS (%s) %s [0x%s]", bytes_repr(obis_info.server_id).c_str(), obis_info.code_repr().c_str(),
           bytes_repr(obis_info.value).c_str());
  for (auto const &sml_listener : sml_listeners_) {
    if ((!sml_listener->server_id.empty()) && (bytes_repr(obis_info.server_id) != sml_listener->server_id))
      continue;
    if (obis_info.code_repr() != sml_listener->obis_code)
      continue;
    sml_listener->publish_val(obis_info);
  }
}

void Sml::dump_config() { ESP_LOGCONFIG(TAG, "SML:"); }

void Sml::register_sml_listener(SmlListener *listener) { sml_listeners_.emplace_back(listener); }

bool check_sml_data(const bytes &buffer) {
  if (buffer.size() < 2) {
    ESP_LOGW(TAG, "Checksum error in received SML data.");
    return false;
  }

  uint16_t crc_received = (buffer.at(buffer.size() - 2) << 8) | buffer.at(buffer.size() - 1);
  uint16_t crc_calculated = crc16(buffer.data() + 8, buffer.size() - 10, 0x6e23, 0x8408, true, true);
  crc_calculated = (crc_calculated >> 8) | (crc_calculated << 8);
  if (crc_received == crc_calculated) {
    ESP_LOGV(TAG, "Checksum verification successful with CRC16/X25.");
    return true;
  }

  crc_calculated = crc16(buffer.data() + 8, buffer.size() - 10, 0xed50, 0x8408);
  if (crc_received == crc_calculated) {
    ESP_LOGV(TAG, "Checksum verification successful with CRC16/KERMIT.");
    return true;
  }

  ESP_LOGW(TAG, "Checksum error in received SML data.");
  return false;
}

uint8_t get_code(uint8_t byte) {
  switch (byte) {
    case 0x1b:
      return 1;
    case 0x01:
      return 2;
    case 0x1a:
      return 3;
    default:
      return 0;
  }
}

}  // namespace sml
}  // namespace esphome
