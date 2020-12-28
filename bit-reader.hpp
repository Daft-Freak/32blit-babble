#pragma once
#include <cstdint>

// bit reading for word data
class BitReader
{
public:
    BitReader(const uint8_t *data, uint32_t len) : ptr(data), end_ptr(data + len)
    {
        buf = (*ptr++ << 8) | (*ptr++);
        num_bits = 16;
    }

    int read_bits(int bits)
    {
        if(num_bits <= 8)
        {
            if(ptr != end_ptr)
            {
                buf |= (*ptr++ << (8 - num_bits));
                num_bits += 8;
            }
        }
        int ret = 0;

        ret = buf >> (16 - bits);
        buf <<= bits;
        num_bits -= bits;

        return ret;
    }

    void skip_bits(int bits)
    {
        if(bits >= num_bits) {
            bits -= num_bits;
            num_bits = 0;
            buf = 0;
        }

        while(bits >= 8) {
            ptr++;
            bits -= 8;
        }

        read_bits(bits);
    }

    bool eof()
    {
        return ptr == end_ptr;
    }

private:
    uint16_t buf;
    int num_bits = 0;
    const uint8_t *ptr, *end_ptr;
};
