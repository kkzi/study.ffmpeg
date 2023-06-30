#pragma once

#include <memory>
#ifdef _MSC_VER
    #define _WIN32_WINNT 0x0501
#endif
#include <boost/asio.hpp>

namespace cortex
{
    // return ms
    const static double parse_crtx_time(int time_code, const unsigned char *data, int index)
    {
        double time = 0.0;
        unsigned int sec, msec, mic;
        memcpy(&sec, data + index, sizeof(int));
        memcpy(&msec, data + index + sizeof(int), sizeof(int));
        memcpy(&mic, data + index + sizeof(int), sizeof(int));
        sec = ntohl(sec);
        msec = ntohl(msec);
        mic = ntohl(mic);

        if (time_code == 0)
        {
            time = (double)sec * 1000 + msec;
        }
        else if (time_code == 1)
        {
            int day = (sec >> 8) & 0xFFFF;
            int hour = sec & 0xFF;
            int minute = ((int)msec >> 24) & 0xFF;
            int second = ((int)msec >> 16) & 0xFF;
            int millisecond = ((int)msec) & 0xFFFF;
            time = (day * 24 * 60 * 60 + hour * 60 * 60 + minute * 60 + second) * 1000.0 + millisecond;
        }
        else if (time_code == 2)
        {
            typedef union
            {
                struct
                {
                    unsigned char byte1;
                    unsigned char byte2;
                    unsigned char byte3;
                    unsigned char byte4;
                } float_byte;

                float value;
            } FLOAT_UNION;

            FLOAT_UNION float_union;
            float_union.float_byte.byte1 = data[index + sizeof(int) + 3];
            float_union.float_byte.byte2 = data[index + sizeof(int) + 2];
            float_union.float_byte.byte3 = data[index + sizeof(int) + 1];
            float_union.float_byte.byte4 = data[index + sizeof(int)];
            time = (double)sec * 1000 + float_union.value;
        }
        else if (time_code == 3)
        {
            time = (double)sec * 1000 + (double)mic / 1e3;
        }
        return time;
    }
}  // namespace cortex