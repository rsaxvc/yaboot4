#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void* calloc (size_t num, size_t size)
{
void * retn = malloc( num * size );
if( retn )
    memset( retn, 0x00, num * size );
return retn;
}
