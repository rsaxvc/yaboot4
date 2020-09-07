#ifndef _REISERFS_SWAB_H_
#define _REISERFS_SWAB_H_
/* Stolen from GCC10 */
#define swab16(x) __builtin_bswap16(x)
#define swab32(x) __builtin_bswap32(x)
#define swab64(x) __builtin_bswap64(x)

#endif /* _REISERFS_SWAB_H_ */
