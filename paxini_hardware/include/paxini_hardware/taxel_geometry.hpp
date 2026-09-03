#ifndef PAXINI_HARDWARE__TAXEL_GEOMETRY_HPP_
#define PAXINI_HARDWARE__TAXEL_GEOMETRY_HPP_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "geometry_msgs/msg/point.hpp"
#include "paxini_hardware/tactile_sensor.hpp"

namespace paxini_hardware
{

/// Physical position of a single taxel, as loaded from the calibration CSV
/// files installed under `share/paxini_hardware/geometry/`.
struct TaxelGeometry
{
  uint32_t index;
  geometry_msgs::msg::Point position;
};

/// Loads and validates the taxel geometry CSV for a single sensor.
/**
 * The CSV is expected at `<paxini_hardware share dir>/geometry/<sensor.name>.csv`
 * with header `index,X,Y,Z` (columns in millimetres, 1-based index), and must
 * contain exactly `sensor.point_count()` rows.
 *
 * \throws std::runtime_error if the file is missing, malformed, or the row
 * count does not match `sensor.point_count()`.
 */
std::vector<TaxelGeometry> load_taxel_geometry(const SensorConfig & sensor);

/// Convenience helper that loads taxel geometry for every sensor returned by
/// `supported_sensors()`, keyed by sensor name.
std::unordered_map<std::string, std::vector<TaxelGeometry>> load_all_taxel_geometry();

}  // namespace paxini_hardware

#endif  // PAXINI_HARDWARE__TAXEL_GEOMETRY_HPP_
