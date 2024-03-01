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
        //virtual ~data_logger();

    protected:

    private:
      uint8_t max_log_count;
      uint8_t log_count;
      uint8_t data_total[2];
      // uint16_t data_range[2][2] = {{1385, 2885}, {0, 3462}};
};

#endif // DATA_LOGGER_H
