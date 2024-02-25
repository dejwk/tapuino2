#include <inttypes.h>

#include "roo_display/core/utf8.h"

namespace tapuino {

inline uint8_t *writeU8(uint8_t v, uint8_t *target) {
  *target++ = v;
  return target;
}

inline uint8_t *writeU16(uint16_t v, uint8_t *target) {
  *target++ = (v >> 8);
  *target++ = (v >> 0) & 0xFF;
  return target;
}

inline uint8_t *writeU24(uint32_t v, uint8_t *target) {
  if (v > 0x00FFFFFF) v = 0x00FFFFFF;
  *target++ = (v >> 16);
  *target++ = (v >> 8) & 0xFF;
  *target++ = (v >> 0) & 0xFF;
  return target;
}

inline uint8_t *writeU32(uint32_t v, uint8_t *target) {
  *target++ = (v >> 24);
  *target++ = (v >> 16) & 0xFF;
  *target++ = (v >> 8) & 0xFF;
  *target++ = (v >> 0) & 0xFF;
  return target;
}

// Puts len in the first byte.
inline uint8_t *writeStr(const char *str, uint8_t len, uint8_t *target) {
  *target++ = len;
  memcpy(target, str, len);
  return target + len;
}

inline const uint8_t* readU8(uint8_t& v, const uint8_t *source) { 
    v = source[0];
    return source + 1;
}

inline const uint8_t* readU16(uint16_t& v, const uint8_t *source) {
  v = (source[0] << 8) | source[1];
  return source + 2;
}

inline const uint8_t* readU24(uint32_t& v, const uint8_t *source) {
  v = (source[0] << 16) | (source[1] << 8) | source[2];
  return source + 3;
}

inline const uint8_t* readU32(uint32_t& v, const uint8_t *source) {
  v = (source[0] << 24) | (source[1] << 16) | (source[2] << 8) | source[3];
  return source + 4;
}

// Expects len in the first byte.
inline const uint8_t* readStr(roo_display::StringView& v, const uint8_t *source) {
  v = roo_display::StringView(source + 1, *source);
  return source + 1 + v.size();
}

}  // namespace tapuino