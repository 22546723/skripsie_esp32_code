#include <cstdint>
#include <tuple>

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

class DataLogger
{
    public:
        explicit DataLogger(uint8_t max_log);
        void setMaxLogs(uint8_t max_log);
        uint8_t getMaxLogs();
        std::tuple<bool, uint8_t, uint8_t> logData(uint16_t data1, uint16_t data2);

    protected:

    private:
      uint8_t max_log_count;
      uint8_t log_count;
      float data_total[2];
};

#endif // DATA_LOGGER_H
