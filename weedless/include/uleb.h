#include <utility>
#include <cstdint>
#include <stdexcept>

// Based on: https://opensource.apple.com/source/dyld/dyld-132.13/src/ImageLoaderMachOCompressed.cpp
static std::pair<uintptr_t, int> read_uleb128(const uint8_t*& p, const uint8_t* end)
{
  uintptr_t result = 0;
  int bit = 0;
  do {
    if (p == end)
      throw std::runtime_error("malformed uleb128");

    uint64_t slice = *p & 0x7f;

    if (bit >= 64 || slice << bit >> bit != slice)
      throw std::runtime_error("uleb128 too big");
    else {
      result |= (slice << bit);
      bit += 7;
    }
  } 
  while (*p++ & 0x80);
  return {result, bit};
}

static uint32_t write_uleb128(uint8_t *p, uint64_t value, uint32_t length_limit) {
    uint8_t *orig = p;
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        
        if (value != 0) {
            byte |= 0x80;
        }
        
        *p++ = byte;
    } while (value != 0);
    
    uint32_t len = (uint32_t)(p - orig);
    
    int32_t pad = length_limit - len;
    if (pad < 0 && length_limit != 0) {
        throw std::runtime_error("length error");
    }
    
    if (pad != 0 && pad > 0) {
        // mark these bytes to show more follow
        for (; pad != 1; --pad) {
            *p++ = '\x80';
        }
        
        // mark terminating byte
        *p++ = '\x00';
    }
    return len;
}
