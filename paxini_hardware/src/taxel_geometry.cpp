#include "paxini_hardware/taxel_geometry.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ament_index_cpp/get_package_share_directory.hpp"

namespace paxini_hardware
{
namespace
{
constexpr double kMillimetresToMetres = 0.001;
}  // namespace

std::vector<TaxelGeometry> load_taxel_geometry(const SensorConfig & sensor)
{
  const auto path = ament_index_cpp::get_package_share_directory("paxini_hardware") +
    "/geometry/" + sensor.name + ".csv";

  std::ifstream stream(path);
  std::string line;
  if (!stream || !std::getline(stream, line) || line != "index,X,Y,Z") {
    throw std::runtime_error("Invalid taxel geometry CSV: " + path);
  }

  std::vector<TaxelGeometry> taxels;
  while (std::getline(stream, line)) {
    std::istringstream row(line);
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(row, field, ',')) {fields.push_back(field);}
    if (fields.size() != 4U) {throw std::runtime_error("Invalid row in " + path);}

    const auto index = static_cast<uint32_t>(std::stoul(fields[0]));
    geometry_msgs::msg::Point position;
    position.x = std::stod(fields[1]) * kMillimetresToMetres;
    position.y = std::stod(fields[2]) * kMillimetresToMetres;
    position.z = std::stod(fields[3]) * kMillimetresToMetres;
    if (index != taxels.size() + 1U || !std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z))
    {
      throw std::runtime_error("Invalid index or coordinate in " + path);
    }
    taxels.push_back({index - 1U, position});
  }

  if (taxels.size() != sensor.point_count()) {
    throw std::runtime_error("Unexpected taxel count in " + path);
  }
  return taxels;
}

std::unordered_map<std::string, std::vector<TaxelGeometry>> load_all_taxel_geometry()
{
  std::unordered_map<std::string, std::vector<TaxelGeometry>> geometry;
  for (const auto & sensor : supported_sensors()) {
    geometry.emplace(sensor.name, load_taxel_geometry(sensor));
  }
  return geometry;
}

}  // namespace paxini_hardware
