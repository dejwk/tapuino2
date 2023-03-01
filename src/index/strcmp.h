#include <inttypes.h>

#include <cstring>

// https://stackoverflow.com/questions/36897781/how-to-uppercase-lowercase-utf-8-characters-in-c

namespace tapuino {

unsigned char* StrToLwrExt(unsigned char* pString);
int StrnCiCmp(const char* s1, const char* s2, size_t ztCount);
int StrCiCmp(const char* s1, const char* s2);
char* StrCiStr(const char* s1, const char* s2);

}  // namespace tapuino