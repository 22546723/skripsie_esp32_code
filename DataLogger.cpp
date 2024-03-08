#include "DataLogger.h"
#include <cstdint>

DataLogger::DataLogger( uint8_t max_log )
  : max_log_count(max_log)
  {
    data_total[0] = 0;
    data_total[1] = 0;
    log_count = 0;
  } // end constructor


/**
 * Set the amount of readings to average and uload to firestore 
 *
 * @param max_log number of readings
 */
void DataLogger::setMaxLogs(uint8_t max_log) {
  max_log_count = max_log; 
}


/**
 * Return the amount of readings to average and upload to firestore 
 *
 * @return number of readings
 */
uint8_t DataLogger::getMaxLogs() {
  return max_log_count;
}


/**
 * Add readings to totals and return the running average
 *
 * @return tuple <has the max logs been reached | data 1 avg | data 2 avg>
 */
std::tuple<bool, uint8_t, uint8_t> DataLogger::logData(uint16_t data1, uint16_t data2) {

  // Update totals
  data_total[0] = data_total[0] + data1;
  data_total[1] = data_total[1] + data2;

  // Calculate running avg
  uint8_t data_out[2];
  data_out[0] = data_total[0]/log_count;
  data_out[1] = data_total[1]/log_count;

  bool max_logs_reached = false;

  // Check if max logs has been reached
  if (log_count >= max_log_count) {
    log_count = 0;
    data_total[0] = 0;
    data_total[1] = 0;
    max_logs_reached = true;
  } else {
    log_count = log_count + 1;
  }

  return std::make_tuple(max_logs_reached, data_out[0], data_out[1]);
}