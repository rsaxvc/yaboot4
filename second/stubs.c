#include <stddef.h>
#include <sys/types.h>

/*
These stub functions are primarily used to satisfy
linker dependencies by e2fsprogs and are not implemented.

If these are required later, we should move to newlib or
other embeddable libC.
*/

void qsort(void *base, size_t nitems, size_t size, int (*compar)(const void *, const void*))
{
}


int open(const char *path, int oflag, ... )
{
return -1;
}

int close(int fildes)
{
return -1;
}

int ioctl(int fd, unsigned long request, ...)
{
return -1;
}

int write(int fd, const void *buf, size_t nbytes)
{
return 0;
}

void * fopen ( const char * filename, const char * mode )
{
return NULL;
}

char * fgets ( char * str, int num, void * stream )
{
return NULL;
}

int fclose ( void * stream )
{
return 0;
}

int unlink(const char *pathname)
{
return -1;
}

int __xstat(int ver, const char * path, void * stat_buf)
{
return -1;
}

int __fxstat (int vers, int fd, void *buf)
{
return -1;
}

void *setmntent(const char *filename, const char *type)
{
return NULL;
}

void *getmntent(void *fp)
{
return NULL;
}

int endmntent(void *fp)
{
return 1;
}

char *hasmntopt(const void *mnt, const char *opt)
{
return NULL;
}

void abort(void)
{
while(1);
}

unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
return 0;
}

