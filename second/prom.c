/*
 *  prom.c - Routines for talking to the Open Firmware PROM
 *
 *  Copyright (C) 2001, 2002 Ethan Benson
 *
 *  Copyright (C) 1999 Benjamin Herrenschmidt
 *
 *  Copyright (C) 1999 Marius Vollmer
 *
 *  Copyright (C) 1996 Paul Mackerras.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "prom.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdlib.h"
#include "types.h"
#include "ctype.h"
#include "asm/processor.h"
#include "errors.h"
#include "debug.h"
#include "string.h"

#define READ_BLOCKS_USE_READ	1

static int yaboot_debug;

prom_entry prom;

ihandle prom_stdin, prom_stdout;

static ihandle prom_mem, prom_mmu;
static ihandle prom_chosen, prom_options;

struct prom_args {
     const char *service;
     int nargs;
     int nret;
     void *args[10];
};

void *
call_prom (const char *service, int nargs, int nret, ...)
{
     va_list list;
     int i;
     struct prom_args prom_args;

     prom_args.service = service;
     prom_args.nargs = nargs;
     prom_args.nret = nret;
     va_start (list, nret);
     for (i = 0; i < nargs; ++i)
	  prom_args.args[i] = va_arg(list, void *);
     va_end(list);
     for (i = 0; i < nret; ++i)
	  prom_args.args[i + nargs] = 0;
     prom (&prom_args);
     if (nret > 0)
	  return prom_args.args[nargs];
     else
	  return 0;
}

void *
call_prom_return (const char *service, int nargs, int nret, ...)
{
     va_list list;
     int i;
     void* result;
     struct prom_args prom_args;

     prom_args.service = service;
     prom_args.nargs = nargs;
     prom_args.nret = nret;
     va_start (list, nret);
     for (i = 0; i < nargs; ++i)
	  prom_args.args[i] = va_arg(list, void *);
     for (i = 0; i < nret; ++i)
	  prom_args.args[i + nargs] = 0;
     if (prom (&prom_args) != 0)
	  return PROM_INVALID_HANDLE;
     if (nret > 0) {
	  result = prom_args.args[nargs];
	  for (i=1; i<nret; i++) {
	       void** rp = va_arg(list, void**);
	       *rp = prom_args.args[i+nargs];
	  }
     } else
	  result = 0;
     va_end(list);
     return result;
}

static void *
call_method_1 (char *method, prom_handle h, int nargs, ...)
{
     va_list list;
     int i;
     struct prom_args prom_args;

     prom_args.service = "call-method";
     prom_args.nargs = nargs+2;
     prom_args.nret = 2;
     prom_args.args[0] = method;
     prom_args.args[1] = h;
     va_start (list, nargs);
     for (i = 0; i < nargs; ++i)
	  prom_args.args[2+i] = va_arg(list, void *);
     va_end(list);
     prom_args.args[2+nargs] = 0;
     prom_args.args[2+nargs+1] = 0;

     prom (&prom_args);

     if (prom_args.args[2+nargs] != 0)
     {
	  prom_printf ("method '%s' failed %p\n", method, prom_args.args[2+nargs]);
	  return 0;
     }
     return prom_args.args[2+nargs+1];
}


prom_handle
prom_finddevice (char *name)
{
     return call_prom ("finddevice", 1, 1, name);
}

prom_handle
prom_findpackage(char *path)
{
     return call_prom ("find-package", 1, 1, path);
}

int
prom_getprop (prom_handle pack, char *name, void *mem, int len)
{
     return (int)call_prom ("getprop", 4, 1, pack, name, mem, len);
}

int
prom_getproplen(prom_handle pack, const char *name)
{
     return (int)call_prom("getproplen", 2, 1, pack, name);
}

int
prom_setprop (prom_handle pack, char *name, void *mem, int len)
{
     return (int)call_prom ("setprop", 4, 1, pack, name, mem, len);
}

int
prom_get_chosen (char *name, void *mem, int len)
{
     return prom_getprop (prom_chosen, name, mem, len);
}

int
prom_get_options (char *name, void *mem, int len)
{
     if (prom_options == (void *)-1)
	  return -1;
     return prom_getprop (prom_options, name, mem, len);
}

int
prom_set_options (char *name, void *mem, int len)
{
     if (prom_options == (void *)-1)
	  return -1;
     return prom_setprop (prom_options, name, mem, len);
}

int
prom_get_devtype (char *device)
{
     phandle    dev;
     int        result;
     char       tmp[64];

     if (strstr(device, TOK_ISCSI))
	  return FILE_DEVICE_ISCSI;

     /* Find OF device phandle */
     dev = prom_finddevice(device);
     if (dev == PROM_INVALID_HANDLE) {
	  return FILE_ERR_BADDEV;
     }

     /* Check the kind of device */
     result = prom_getprop(dev, "device_type", tmp, 63);
     if (result == -1) {
	  prom_printf("can't get <device_type> for device: %s\n", device);
	  return FILE_ERR_BADDEV;
     }
     tmp[result] = 0;
     if (!strcmp(tmp, "block"))
	  return FILE_DEVICE_BLOCK;
     else if (!strcmp(tmp, "network"))
	  return FILE_DEVICE_NET;
     else {
	  prom_printf("Unknown device type <%s>\n", tmp);
	  return FILE_ERR_BADDEV;
     }
}

void
prom_init (prom_entry pp)
{
     prom = pp;

     prom_chosen = prom_finddevice ("/chosen");
     if (prom_chosen == (void *)-1)
	  prom_exit ();
     prom_options = prom_finddevice ("/options");
     if (prom_get_chosen ("stdout", &prom_stdout, sizeof(prom_stdout)) <= 0)
	  prom_exit();
     if (prom_get_chosen ("stdin", &prom_stdin, sizeof(prom_stdin)) <= 0)
	  prom_abort ("\nCan't open stdin");
     if (prom_get_chosen ("memory", &prom_mem, sizeof(prom_mem)) <= 0)
	  prom_abort ("\nCan't get mem handle");
     if (prom_get_chosen ("mmu", &prom_mmu, sizeof(prom_mmu)) <= 0)
	  prom_abort ("\nCan't get mmu handle");

     yaboot_debug = 0;
     prom_get_options("linux,yaboot-debug", &yaboot_debug, sizeof(yaboot_debug));

  // move cursor to fresh line
     prom_printf ("\n");

     /* Add a few OF methods (thanks Darwin) */
#if DEBUG
     prom_printf ("Adding OF methods...\n");
#endif

     prom_interpret (
	  /* All values in this forth code are in hex */
	  "hex "
	  /* Those are a few utilities ripped from Apple */
	  ": D2NIP decode-int nip nip ;\r"	// A useful function to save space
	  ": GPP$ get-package-property 0= ;\r"	// Another useful function to save space
	  ": ^on0 0= if -1 throw then ;\r"	// Bail if result zero
	  ": $CM $call-method ;\r"
	  );

     /* Some forth words used by the release method */
     prom_interpret (
	  " \" /chosen\" find-package if "
		 "dup \" memory\" rot GPP$ if "
			 "D2NIP swap "				 // ( MEMORY-ihandle "/chosen"-phandle )
			 "\" mmu\" rot GPP$ if "
				 "D2NIP "				 // ( MEMORY-ihandle MMU-ihandle )
			 "else "
				 "0 "					 // ( MEMORY-ihandle 0 )
			 "then "
		 "else "
			 "0 0 "						 // ( 0 0 )
		 "then "
	  "else "
		 "0 0 "							 // ( 0 0 )
	  "then\r"
	  "value mmu# "
	  "value mem# "
	  );

     prom_interpret (
	  ": ^mem mem# $CM ; "
	  ": ^mmu mmu# $CM ; "
	  );

     DEBUG_F("OF interface initialized.\n");
}

prom_handle
prom_open (char *spec)
{
     return call_prom ("open", 1, 1, spec, strlen(spec));
}

void
prom_close (prom_handle file)
{
     call_prom ("close", 1, 0, file);
}

int
prom_read (prom_handle file, void *buf, int n)
{
     int result = 0;
     int retries = 10;

     if (n == 0)
	  return 0;
     while(--retries) {
	  result = (int)call_prom ("read", 3, 1, file, buf, n);
	  if (result != 0)
	       break;
	  call_prom("interpret", 1, 1, " 10 ms");
     }

     return result;
}

int
prom_write (prom_handle file, void *buf, int n)
{
     return (int)call_prom ("write", 3, 1, file, buf, n);
}

int
prom_seek (prom_handle file, int pos)
{
     int status = (int)call_prom ("seek", 3, 1, file, 0, pos);
     return status == 0 || status == 1;
}

int
prom_lseek (prom_handle file, unsigned long long pos)
{
     int status = (int)call_prom ("seek", 3, 1, file,
				  (unsigned int)(pos >> 32), (unsigned int)(pos & 0xffffffffUL));
     return status == 0 || status == 1;
}

int
prom_loadmethod (prom_handle device, void* addr)
{
     return (int)call_method_1 ("load", device, 1, addr);
}

int
prom_getblksize (prom_handle file)
{
     return (int)call_method_1 ("block-size", file, 0);
}

int
prom_readblocks (prom_handle dev, int blockNum, int blockCount, void *buffer)
{
#if READ_BLOCKS_USE_READ
     int status;
     unsigned int blksize;

     blksize = prom_getblksize(dev);
     if (blksize <= 1)
	  blksize = 512;
     status = prom_seek(dev, blockNum * blksize);
     if (status != 1) {
	  return 0;
	  prom_printf("Can't seek to 0x%x\n", blockNum * blksize);
     }

     status = prom_read(dev, buffer, blockCount * blksize);
//  prom_printf("prom_readblocks, bl: %d, cnt: %d, status: %d\n",
//  	blockNum, blockCount, status);

     return status == (blockCount * blksize);
#else
     int result;
     int retries = 10;

     if (blockCount == 0)
	  return blockCount;
     while(--retries) {
	  result = call_method_1 ("read-blocks", dev, 3, buffer, blockNum, blockCount);
	  if (result != 0)
	       break;
	  call_prom("interpret", 1, 1, " 10 ms");
     }

     return result;
#endif
}

int
prom_getchar ()
{
     char c;
     int a;

     while ((a = (int)call_prom ("read", 3, 1, prom_stdin, &c, 1)) == 0)
	  ;
     if (a == -1)
	  prom_abort ("EOF on console\n");
     return c;
}

int
prom_nbgetchar()
{
     char ch;

     return (int) call_prom("read", 3, 1, prom_stdin, &ch, 1) > 0? ch: -1;
}

void
prom_putchar (char c)
{
     if (c == '\n')
	  call_prom ("write", 3, 1, prom_stdout, "\r\n", 2);
     else
	  call_prom ("write", 3, 1, prom_stdout, &c, 1);
}

void
prom_puts (prom_handle file, char *s)
{
     const char *p, *q;

     for (p = s; *p != 0; p = q)
     {
	  for (q = p; *q != 0 && *q != '\n'; ++q)
	       ;
	  if (q > p)
	       call_prom ("write", 3, 1, file, p, q - p);
	  if (*q != 0)
	  {
	       ++q;
	       call_prom ("write", 3, 1, file, "\r\n", 2);
	  }
     }
}

void
prom_vfprintf (prom_handle file, const char *fmt, va_list ap)
{
     static char printf_buf[2048];
     vsprintf (printf_buf, fmt, ap);
     prom_puts (file, printf_buf);
}

void
prom_vprintf (const char *fmt, va_list ap)
{
     static char printf_buf[2048];
     vsprintf (printf_buf, fmt, ap);
     prom_puts (prom_stdout, printf_buf);
}

void
prom_fprintf (prom_handle file, const char *fmt, ...)
{
     va_list ap;
     va_start (ap, fmt);
     prom_vfprintf (file, fmt, ap);
     va_end (ap);
}

void
prom_printf (const char *fmt, ...)
{
     va_list ap;
     va_start (ap, fmt);
     prom_vfprintf (prom_stdout, fmt, ap);
     va_end (ap);
}

void
prom_debug (const char *fmt, ...)
{
     va_list ap;

     if (!yaboot_debug)
          return;

     va_start (ap, fmt);
     prom_vfprintf (prom_stdout, fmt, ap);
     va_end (ap);
}

void
prom_perror (int error, char *filename)
{
     if (error == FILE_ERR_EOF)
	  prom_printf("%s: Unexpected End Of File\n", filename);
     else if (error == FILE_ERR_NOTFOUND)
	  prom_printf("%s: No such file or directory\n", filename);
     else if (error == FILE_CANT_SEEK)
	  prom_printf("%s: Seek error\n", filename);
     else if (error == FILE_IOERR)
	  prom_printf("%s: Input/output error\n", filename);
     else if (error == FILE_BAD_PATH)
	  prom_printf("%s: Path too long\n", filename);
     else if (error == FILE_ERR_BAD_TYPE)
	  prom_printf("%s: Not a regular file\n", filename);
     else if (error == FILE_ERR_NOTDIR)
	  prom_printf("%s: Not a directory\n", filename);
     else if (error == FILE_ERR_BAD_FSYS)
	  prom_printf("%s: Unknown or corrupt filesystem\n", filename);
     else if (error == FILE_ERR_SYMLINK_LOOP)
	  prom_printf("%s: Too many levels of symbolic links\n", filename);
     else if (error == FILE_ERR_LENGTH)
	  prom_printf("%s: File too large\n", filename);
     else if (error == FILE_ERR_FSBUSY)
	  prom_printf("%s: Filesystem busy\n", filename);
     else if (error == FILE_ERR_BADDEV)
	  prom_printf("%s: Unable to open file, Invalid device\n", filename);
     else
	  prom_printf("%s: Unknown error\n", filename);
}

void
prom_readline (char *prompt, char *buf, int len)
{
     int i = 0;
     int c;

     if (prompt)
	  prom_puts (prom_stdout, prompt);

     while (i < len-1 && (c = prom_getchar ()) != '\r')
     {
	  if (c == 8)
	  {
	       if (i > 0)
	       {
		    prom_puts (prom_stdout, "\b \b");
		    i--;
	       }
	       else
		    prom_putchar ('\a');
	  }
	  else if (isprint (c))
	  {
	       prom_putchar (c);
	       buf[i++] = c;
	  }
	  else
	       prom_putchar ('\a');
     }
     prom_putchar ('\n');
     buf[i] = 0;
}

#ifdef CONFIG_SET_COLORMAP
int prom_set_color(prom_handle device, int color, int r, int g, int b)
{
     return (int)call_prom( "call-method", 6, 1, "color!", device, color, b, g, r );
}
#endif /* CONFIG_SET_COLORMAP */

void
prom_exit ()
{
     call_prom ("exit", 0, 0);
}

void
prom_abort (char *fmt, ...)
{
     va_list ap;
     va_start (ap, fmt);
     prom_vfprintf (prom_stdout, fmt, ap);
     va_end (ap);
     prom_exit ();
}

void
prom_sleep (int seconds)
{
     int end;
     end = (prom_getms() + (seconds * 1000));
     while (prom_getms() <= end);
}

/* if address given is claimed look for other addresses to get the needed
 * space before giving up
 */
void *
prom_claim_chunk(void *virt, unsigned int size, unsigned int align)
{
     void *found, *addr;
     for(addr=virt; addr <= (void*)PROM_CLAIM_MAX_ADDR;
         addr+=(0x100000/sizeof(addr))) {
          found = call_prom("claim", 3, 1, addr, size, 0);
          if (found != (void *)-1) {
               prom_debug("claim of 0x%x at 0x%x returned 0x%x\n", size, (int)addr, (int)found);
               return(found);
          }
     }
     prom_printf("ERROR: claim of 0x%x in range 0x%x-0x%x failed\n", size, (int)virt, PROM_CLAIM_MAX_ADDR);
     return((void*)-1);
}

/* Start from top of memory and work down to get the needed space */
void *
prom_claim_chunk_top(unsigned int size, unsigned int align)
{
     void *found, *addr;
     for(addr=(void*)PROM_CLAIM_MAX_ADDR; addr >= (void *)size;
         addr-=(0x100000/sizeof(addr))) {
          found = call_prom("claim", 3, 1, addr, size, 0);
          if (found != (void *)-1) {
               prom_debug("claim of 0x%x at 0x%x returned 0x%x\n", size, (int)addr, (int)found);
               return(found);
          }
     }
     prom_printf("ERROR: claim of 0x%x in range 0x0-0x%x failed\n", size, PROM_CLAIM_MAX_ADDR);
     return((void*)-1);
}

void *
prom_claim (void *virt, unsigned int size, unsigned int align)
{
     void *ret;

     ret = call_prom ("claim", 3, 1, virt, size, align);
     if (ret == (void *)-1)
          prom_printf("ERROR: claim of 0x%x at 0x%x failed\n", size, (int)virt);
     else
          prom_debug("claim of 0x%x at 0x%x returned 0x%x\n", size, (int)virt, (int)ret);

     return ret;
}

void
prom_release(void *virt, unsigned int size)
{
     void *ret;

     ret = call_prom ("release", 2, 0, virt, size);
     prom_debug("release of 0x%x at 0x%x returned 0x%x\n", size, (int)virt, (int)ret);
}

void
prom_map (void *phys, void *virt, int size)
{
     unsigned long msr = mfmsr();

     /* Only create a mapping if we're running with relocation enabled. */
     if ( (msr & MSR_IR) && (msr & MSR_DR) )
	  call_method_1 ("map", prom_mmu, 4, -1, size, virt, phys);
}

void
prom_unmap (void *phys, void *virt, int size)
{
     unsigned long msr = mfmsr();

     /* Only unmap if we're running with relocation enabled. */
     if ( (msr & MSR_IR) && (msr & MSR_DR) )
	  call_method_1 ("map", prom_mmu, 4, -1, size, virt, phys);
}

char *
prom_getargs ()
{
     static char args[256];
     int l;

     l = prom_get_chosen ("bootargs", args, 255);
     args[l] = '\0';
     return args;
}

void
prom_setargs (char *args)
{
     int l = strlen (args)+1;
     if ((int)call_prom ("setprop", 4, 1, prom_chosen, "bootargs", args, l) != l)
	  prom_printf ("can't set args\n");
}

int prom_interpret (char *forth)
{
     return (int)call_prom("interpret", 1, 1, forth);
}

int
prom_getms(void)
{
     return (int) call_prom("milliseconds", 0, 1);
}

void
prom_pause(void)
{
     prom_print_available();
     call_prom("enter", 0, 0);
}

/*
 * prom_get_netinfo()
 * returns the packet with all needed info for netboot
 */
struct bootp_packet * prom_get_netinfo (void)
{
     void *bootp_response = NULL;
     char *propname;
     struct bootp_packet *packet;
     /* struct bootp_packet contains a VLA, so sizeof won't work.
        the VLA /must/ be the last field in the structure so use it's
        offset as a good estimate of the packet size */
     size_t packet_size = offsetof(struct bootp_packet, options);
     int i = 0, size, offset = 0;
     prom_handle chosen;
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

     chosen = prom_finddevice("/chosen");
     if (chosen < 0) {
          DEBUG_F("chosen=%lu\n", (unsigned long)chosen);
      return 0;
     }

     for (i = 0; i < ARRAY_SIZE(bootp_response_properties); i++) {
         propname = bootp_response_properties[i].name;
         size = prom_getproplen(chosen, propname);
         if (size <= 0)
             continue;

         DEBUG_F("using /chosen/%s\n", propname);
         offset = bootp_response_properties[i].offset;
         break;
     }

     if (size <= 0)
         return NULL;

     if (packet_size > size - offset) {
         prom_printf("Malformed %s property?\n", propname);
         return NULL;
     }

     bootp_response = malloc(size);
     if (!bootp_response)
         return NULL;

     if (prom_getprop(chosen, propname, bootp_response, size) < 0)
         return NULL;

     packet = bootp_response + offset;
     return packet;
}

/*
 * prom_get_mac()
 * returns the mac addr of an net card
 */
char * prom_get_mac (struct bootp_packet * packet)
{
     char * conf_path;
     int i;

     if (!packet)
        return NULL;

     /* 3 chars per byte in chaddr + \0 */
     conf_path = malloc(packet->hlen * 3 + 1);
     if (!conf_path)
         return NULL;
     sprintf(conf_path, "%02x", packet->chaddr[0]);

     for (i = 1; i < packet->hlen; i++) {
      char tmp[4];
      sprintf(tmp, "-%02x", packet->chaddr[i]);
      strcat(conf_path, tmp);
     }

     return conf_path;
}

/*
 * prom_get_ip()
 * returns the ip addr of an net card
 */
char * prom_get_ip (struct bootp_packet * packet)
{
     char * conf_path;

     if (!packet)
        return NULL;

     /* 8 chars in yiaddr + \0 */
     conf_path = malloc(9);
     if (!conf_path)
         return NULL;
     sprintf(conf_path, "%08x", packet->yiaddr);

     return conf_path;
}

/* We call this too early to use malloc, 128 cells should be large enough */
#define NR_AVAILABLE 128

void prom_print_available(void)
{
     prom_handle root;
     unsigned int addr_cells, size_cells;
     ihandle mem;
     unsigned int available[NR_AVAILABLE];
     unsigned int len;
     unsigned int *p;

     if (!yaboot_debug)
          return;

     root = prom_finddevice("/");
     if (!root)
          return;

     addr_cells = 2;
     prom_getprop(root, "#address-cells", &addr_cells, sizeof(addr_cells));

     size_cells = 1;
     prom_getprop(root, "#size-cells", &size_cells, sizeof(size_cells));

     mem = prom_finddevice("/memory@0");
     if (mem == PROM_INVALID_HANDLE)
          return;

     len = prom_getprop(mem, "available", available, sizeof(available));
     if (len == -1)
          return;
     len /= 4;

     prom_printf("\nAvailable memory ranges:\n");

     p = available;
     while (len > 0) {
          unsigned int addr, size;

          /*
           * Since we are in 32bit mode it should be safe to only print the
           * bottom 32bits of each range.
           */
          p += (addr_cells - 1);
          addr = *p;
          p++;

          p += (size_cells - 1);
          size = *p;
          p++;

          if (size)
               prom_printf("0x%08x-0x%08x (%3d MB)\n", addr, addr + size,
                           size/1024/1024);

          len -= (addr_cells + size_cells);
     }

     prom_printf("\n");
}

/*
 * Local variables:
 * c-file-style: "k&r"
 * c-basic-offset: 5
 * End:
 */
