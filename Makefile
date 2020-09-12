## Setup

include Config

VERSION = 1.4.00
# Debug mode (spam/verbose)
DEBUG = 0
# make install vars
ROOT =
PREFIX = usr/local
MANDIR = man
# command used to get root (needed for tarball creation)
GETROOT = fakeroot

# We use fixed addresses to avoid overlap when relocating
# and other trouble with initrd

# Load the bootstrap at 1Mb
TEXTADDR	= 0x100000
# Malloc block of 1MB
MALLOCSIZE	= 0x100000
# Load kernel and ramdisk at as low as possible
KERNELADDR	= 0x00000000

# Set this to the prefix of your cross-compiler, if you have one.
# Else leave it empty.
#
CROSS =

CC		:= $(CROSS)gcc
LD		:= $(CROSS)ld
AS		:= $(CROSS)as
OBJCOPY		:= $(CROSS)objcopy

# The flags for the yaboot binary.
#
YBCFLAGS = -Os -m32 $(CFLAGS) -nostdinc -Wall -isystem `$(CC) -m32 -print-file-name=include` -fsigned-char -ffunction-sections
YBCFLAGS += -I e2fsprogs/lib/
YBCFLAGS += -I e2fsprogs/lib/ext2fs/
YBCFLAGS += -I e2fsprogs/build/lib/
YBCFLAGS += -DNO_INLINE_FUNCS
YBCFLAGS += -DVERSION="\"${VERSION}${VERSIONEXTRA}\""
YBCFLAGS += -DTEXTADDR=$(TEXTADDR) -DDEBUG=$(DEBUG)
YBCFLAGS += -DMALLOCADDR=$(MALLOCADDR) -DMALLOCSIZE=$(MALLOCSIZE)
YBCFLAGS += -DKERNELADDR=$(KERNELADDR)
YBCFLAGS += -Ddev_t=uint32_t
YBCFLAGS += -Dtime_t=uint32_t
YBCFLAGS += -fgnu89-inline -fno-builtin-malloc
YBCFLAGS += -I ./include
YBCFLAGS += -fno-strict-aliasing

ifeq ($(CONFIG_COLOR_TEXT),y)
YBCFLAGS += -DCONFIG_COLOR_TEXT
endif

ifeq ($(CONFIG_SET_COLORMAP),y)
YBCFLAGS += -DCONFIG_SET_COLORMAP
endif

ifeq ($(USE_MD5_PASSWORDS),y)
YBCFLAGS += -DUSE_MD5_PASSWORDS
endif

ifeq ($(CONFIG_FS_XFS),y)
YBCFLAGS += -DCONFIG_FS_XFS
endif

ifeq ($(CONFIG_FS_REISERFS),y)
YBCFLAGS += -DCONFIG_FS_REISERFS
endif

# Link flags
#
LFLAGS = -Ttext $(TEXTADDR) -Bstatic -melf32ppclinux --gc-sections

# Libraries
#
E2FSLIB = e2fsprogs/build/lib/libext2fs.a

# For compiling userland utils
#
UCFLAGS = -Os -g $(CFLAGS) -Wall -I/usr/include
UCFLAGS += -fstack-protector-strong
UCFLAGS += -D_FORTIFY_SOURCE=2
UCFLAGS += -Wl,-z,relro
UCFLAGS += -Werror -fdiagnostics-show-option

# For compiling build-tools that run on the host.
#
HOSTCC = gcc
HOSTCFLAGS = -O2 $(CFLAGS) -Wall -I/usr/include

## End of configuration section

OBJS = second/crt0.o second/yaboot.o second/cache.o second/prom.o second/file.o \
	second/partition.o second/fs.o second/cfg.o second/setjmp.o second/cmdline.o \
	second/errno_location.o second/fs_of.o second/fs_ext2.o second/fs_iso.o second/fs_swap.o \
	second/iso_util.o \
	second/calloc.o \
	second/strnlen.o \
	second/stubs.o \
	lib/nonstd.o \
	lib/nosys.o lib/string.o lib/strtol.o lib/vsprintf.o lib/ctype.o lib/malloc.o lib/strstr.o

ifeq ($(USE_MD5_PASSWORDS),y)
OBJS += second/md5.o
endif

ifeq ($(CONFIG_FS_XFS),y)
OBJS += second/fs_xfs.o
endif

ifeq ($(CONFIG_FS_REISERFS),y)
OBJS += second/fs_reiserfs.o
endif

# compilation
lgcc = `$(CC) -m32 -print-libgcc-file-name`

all: yaboot addnote mkofboot

yaboot: $(OBJS) $(E2FSLIB)
	$(LD) $(LFLAGS) $(OBJS) $(E2FSLIB) $(lgcc) -o second/$@
	chmod -x second/yaboot

addnote:
	$(CC) $(UCFLAGS) -o util/addnote util/addnote.c

elfextract:
	$(CC) $(UCFLAGS) -o util/elfextract util/elfextract.c

mkofboot:
	ln -sf ybin ybin/mkofboot
	@if [ $$(grep '^VERSION=' ybin/ybin | cut -f2 -d=) != ${VERSION} ] ; then	\
		echo "ybin/ybin: warning: VERSION  mismatch"; 				\
		false; 									\
	fi

#We need some headers built during the e2fsprogs build process.
#Depend on the e2fsprogs library to force e2fsprogs to go first
%.o: %.c $(E2FSLIB)
	$(CC) $(YBCFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(YBCFLAGS) -D__ASSEMBLY__  -c -o $@ $<

e2fsprogs:
	git submodule update --init e2fsprogs

e2fsprogs/build/Makefile: e2fsprogs
	mkdir -p e2fsprogs/build;cd e2fsprogs/build;../configure --disable-mmp --disable-defrag --disable-bmap-stats

$(E2FSLIB): e2fsprogs/build/Makefile
	make -C e2fsprogs/build/

dep:
	makedepend -Iinclude *.c lib/*.c util/*.c gui/*.c

docs:
	make -C doc all

bindist: all
	mkdir ../yaboot-binary-${VERSION}
	$(GETROOT) make ROOT=../yaboot-binary-${VERSION} install
	mkdir -p -m 755 ../yaboot-binary-${VERSION}/usr/local/share/doc/yaboot
	cp -a COPYING ../yaboot-binary-${VERSION}/usr/local/share/doc/yaboot/COPYING
	cp -a README ../yaboot-binary-${VERSION}/usr/local/share/doc/yaboot/README
	cp -a doc/README.rs6000 ../yaboot-binary-${VERSION}/usr/local/share/doc/yaboot/README.rs6000
	cp -a doc/yaboot-howto.html ../yaboot-binary-${VERSION}/usr/local/share/doc/yaboot/yaboot-howto.html
	cp -a doc/yaboot-howto.sgml ../yaboot-binary-${VERSION}/usr/local/share/doc/yaboot/yaboot-howto.sgml
	mv ../yaboot-binary-${VERSION}/etc/yaboot.conf ../yaboot-binary-${VERSION}/usr/local/share/doc/yaboot/
	rmdir ../yaboot-binary-${VERSION}/etc
	$(GETROOT) tar -C ../yaboot-binary-${VERSION} -zcvpf ../yaboot-binary-${VERSION}.tar.gz .
	rm -rf ../yaboot-binary-${VERSION}

clean:
	rm -f second/yaboot util/addnote util/elfextract $(OBJS)
	find . -not -path './\{arch\}*' -name '#*' | xargs rm -f
	find . -not -path './\{arch\}*' -name '.#*' | xargs rm -f
	find . -not -path './\{arch\}*' -name '*~' | xargs rm -f
	find . -not -path './\{arch\}*' -name '*.swp' | xargs rm -f
	find . -not -path './\{arch\}*' -name ',,*' | xargs rm -rf
	-gunzip man/*.gz
	rm -rf man.deb

cleandocs:
	make -C doc clean

## removes arch revision control crap, only to be called for making
## release tarballs.  arch should have a export command like cvs...

archclean:
	rm -rf '{arch}'
	find . -type d -name .arch-ids | xargs rm -rf
	rm -f 0arch-timestamps0

maintclean: clean cleandocs

release: docs bindist clean

strip: all
	strip second/yaboot
	strip --remove-section=.comment second/yaboot
	strip util/addnote
	strip --remove-section=.comment --remove-section=.note util/addnote

install: all 
	install -d -o root -g root -m 0755 ${ROOT}/etc/
	install -d -o root -g root -m 0755 ${ROOT}/${PREFIX}/sbin/
	install -d -o root -g root -m 0755 ${ROOT}/${PREFIX}/lib
	install -d -o root -g root -m 0755 ${ROOT}/${PREFIX}/lib/yaboot
	install -d -o root -g root -m 0755 ${ROOT}/${PREFIX}/${MANDIR}/man5/
	install -d -o root -g root -m 0755 ${ROOT}/${PREFIX}/${MANDIR}/man8/
	install -o root -g root -m 0644 second/yaboot ${ROOT}/$(PREFIX)/lib/yaboot
	install -o root -g root -m 0755 util/addnote ${ROOT}/${PREFIX}/lib/yaboot/addnote
	install -o root -g root -m 0644 first/ofboot ${ROOT}/${PREFIX}/lib/yaboot/ofboot
	install -o root -g root -m 0755 ybin/ofpath ${ROOT}/${PREFIX}/sbin/ofpath
	install -o root -g root -m 0755 ybin/ybin ${ROOT}/${PREFIX}/sbin/ybin
	install -o root -g root -m 0755 ybin/yabootconfig ${ROOT}/${PREFIX}/sbin/yabootconfig
	rm -f ${ROOT}/${PREFIX}/sbin/mkofboot
	ln -s ybin ${ROOT}/${PREFIX}/sbin/mkofboot
	@gzip -9 man/*.[58]
	install -o root -g root -m 0644 man/bootstrap.8.gz ${ROOT}/${PREFIX}/${MANDIR}/man8/bootstrap.8.gz
	install -o root -g root -m 0644 man/mkofboot.8.gz ${ROOT}/${PREFIX}/${MANDIR}/man8/mkofboot.8.gz
	install -o root -g root -m 0644 man/ofpath.8.gz ${ROOT}/${PREFIX}/${MANDIR}/man8/ofpath.8.gz
	install -o root -g root -m 0644 man/yaboot.8.gz ${ROOT}/${PREFIX}/${MANDIR}/man8/yaboot.8.gz
	install -o root -g root -m 0644 man/yabootconfig.8.gz ${ROOT}/${PREFIX}/${MANDIR}/man8/yabootconfig.8.gz
	install -o root -g root -m 0644 man/ybin.8.gz ${ROOT}/${PREFIX}/${MANDIR}/man8/ybin.8.gz
	install -o root -g root -m 0644 man/yaboot.conf.5.gz ${ROOT}/${PREFIX}/${MANDIR}/man5/yaboot.conf.5.gz
	@gunzip man/*.gz
	@if [ ! -e ${ROOT}/etc/yaboot.conf ] ; then						\
		echo "install -o root -g root -m 0644 etc/yaboot.conf ${ROOT}/etc/yaboot.conf"; \
		install -o root -g root -m 0644 etc/yaboot.conf ${ROOT}/etc/yaboot.conf;	\
	 else											\
		echo "/etc/yaboot.conf already exists, leaving it alone";			\
	 fi
	@echo
	@echo "Installation successful."
	@echo
	@echo "An example /etc/yaboot.conf has been installed (unless /etc/yaboot.conf already existed)"
	@echo "You may either alter that file to match your system, or alternatively run yabootconfig"
	@echo "yabootconfig will generate a simple and valid /etc/yaboot.conf for your system"
	@echo

deinstall:
	rm -f ${ROOT}/${PREFIX}/sbin/ofpath
	rm -f ${ROOT}/${PREFIX}/sbin/ybin
	rm -f ${ROOT}/${PREFIX}/sbin/yabootconfig
	rm -f ${ROOT}/${PREFIX}/sbin/mkofboot
	rm -f ${ROOT}/${PREFIX}/lib/yaboot/yaboot
	rm -f ${ROOT}/${PREFIX}/lib/yaboot/ofboot
	rm -f ${ROOT}/${PREFIX}/lib/yaboot/addnote
	@rmdir ${ROOT}/${PREFIX}/lib/yaboot || true
	rm -f ${ROOT}/${PREFIX}/${MANDIR}/man8/bootstrap.8.gz
	rm -f ${ROOT}/${PREFIX}/${MANDIR}/man8/mkofboot.8.gz
	rm -f ${ROOT}/${PREFIX}/${MANDIR}/man8/ofpath.8.gz
	rm -f ${ROOT}/${PREFIX}/${MANDIR}/man8/yaboot.8.gz
	rm -f ${ROOT}/${PREFIX}/${MANDIR}/man8/yabootconfig.8.gz
	rm -f ${ROOT}/${PREFIX}/${MANDIR}/man8/ybin.8.gz
	rm -f ${ROOT}/${PREFIX}/${MANDIR}/man5/yaboot.conf.5.gz
	@if [ -L ${ROOT}/boot/yaboot -a ! -e ${ROOT}/boot/yaboot ] ; then rm -f ${ROOT}/boot/yaboot ; fi
	@if [ -L ${ROOT}/boot/ofboot.b -a ! -e ${ROOT}/boot/ofboot.b ] ; then rm -f ${ROOT}/boot/ofboot.b ; fi
	@echo
	@echo "Deinstall successful."
	@echo "${ROOT}/etc/yaboot.conf has not been removed, you may remove it yourself if you wish."

uninstall: deinstall
