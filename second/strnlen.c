#include <string.h>
size_t strnlen(const char *str, size_t maxlen)
{
size_t i;
for( i = 0; i < maxlen && str[i]; ++i ){};
return i;
}
