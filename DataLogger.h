#include <cstdint>
#include <tuple>

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

class DataLogger
{
    public:
        explicit DataLogger(); 
        void logData(uint16_t soil_data, uint16_t uv_data);
        std::tuple<uint8_t, uint8_t> getData();
        void resetLogger();

    protected:

    private:
      uint32_t log_count;
      float data_total[2];
};

#endif // DATA_LOGGER_H
