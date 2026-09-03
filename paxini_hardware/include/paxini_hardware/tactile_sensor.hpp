#ifndef PAXINI_HARDWARE__TACTILE_SENSOR_HPP_
#define PAXINI_HARDWARE__TACTILE_SENSOR_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace paxini_hardware
{

struct SensorConfig
{
  std::string name;
  uint8_t module_id;
  uint16_t distributed_length;

  uint8_t device_address() const;
  std::size_t point_count() const;
};

const std::vector<SensorConfig> & supported_sensors();

class TactileSensorBus
{
public:
  TactileSensorBus();
  ~TactileSensorBus();

  TactileSensorBus(const TactileSensorBus &) = delete;
  TactileSensorBus & operator=(const TactileSensorBus &) = delete;

  void open(const std::string & device, int baud_rate);
  void close();
  bool is_open() const;
  std::vector<float> read_distributed(const SensorConfig & sensor, int timeout_ms = 250);

private:
  int file_descriptor_;

  void write_all(const std::vector<uint8_t> & command) const;
  std::vector<uint8_t> read_response(std::size_t payload_length, int timeout_ms) const;
};

}  // namespace paxini_hardware

#endif  // PAXINI_HARDWARE__TACTILE_SENSOR_HPP_
