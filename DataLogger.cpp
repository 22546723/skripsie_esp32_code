#include "DataLogger.h"
#include <cstdint>

DataLogger::DataLogger( uint8_t max_log )
  : max_log_count(max_log)
  {
    data_total[0] = 0;
    data_total[1] = 0;
    log_count = 0;
  } // end constructor

void DataLogger::setMaxLogs(uint8_t max_log) {
  max_log_count = max_log; 
}

uint8_t DataLogger::getMaxLogs() {
  return max_log_count;
}

std::tuple<bool, uint8_t, uint8_t> DataLogger::logData(uint16_t data1, uint16_t data2) {

  data_total[0] = data_total[0] + data1;
  data_total[1] = data_total[1] + data2;

  uint8_t data_out[2] = {0, 0};
  bool max_logs_reached = false;

  if (log_count >= max_log_count) {
    data_out[0] = data_total[0]/log_count;
    data_out[1] = data_total[1]/log_count;
    log_count = 0;
    data_total[0] = 0;
    data_total[1] = 0;
    max_logs_reached = true;
  } else {
    log_count = log_count + 1;
  }

  return std::make_tuple(max_logs_reached, data_out[0], data_out[1]);
}