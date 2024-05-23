#include "DataLogger.h"
#include <cstdint>

/**
 * Class that manages the data totals and calculates the averages
 */
DataLogger::DataLogger() {
  
    data_total[0] = 0;
    data_total[1] = 0;
    log_count = 0;
  }


/**
 * Add the data to the totals
 *
 * @param soil_data Soil moisture reading
 * @param uv_data UV exposure reading
 */
void DataLogger::logData(uint16_t soil_data, uint16_t uv_data) {
  // Update totals
  data_total[0] = data_total[0] + soil_data;
  data_total[1] = data_total[1] + uv_data;
  log_count = log_count + 1;
}


/**
 * Calculate the average soil moisture and UV exposure levels
 *
 * @return tuple containing <soil moisture average, uv exposure average>
 */
std::tuple<uint8_t, uint8_t> DataLogger::getData() {
  // Calculate running avg
  uint8_t data_out[2];
  data_out[0] = data_total[0]/log_count;
  data_out[1] = data_total[1]/log_count;

  // reset the logger
  data_total[0] = 0;
  data_total[1] = 0;
  log_count = 0;

  return std::make_tuple(data_out[0], data_out[1]);
}


/**
 * Reset the totals and log counter to zero
 */
void DataLogger::resetLogger() {
  data_total[0] = 0;
  data_total[1] = 0;
  log_count = 0;
}