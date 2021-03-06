# Administration utilities Makefile

if GSS
SUBDIRS = gss
endif

AM_CFLAGS=$(LLCFLAGS)
AM_CPPFLAGS=$(LLCPPFLAGS) -DLUSTRE_UTILS=1
AM_LDFLAGS := -L$(top_builddir)/lnet/utils

LIBPTLCTL := $(top_builddir)/lnet/utils/libptlctl.a \
	     $(top_builddir)/libcfs/libcfs/libcfsutil.a \
	     $(top_builddir)/libcfs/libcfs/libcfs.a

sbin_scripts = lrun
bin_scripts = llstat llobdstat plot-llstat llbackup

if UTILS
noinst_PROGRAMS = obdio obdbarrier
EXTRA_PROGRAMS = wirecheck

# mount only finds helpers in /sbin
rootsbin_PROGRAMS = mount.lustre
sbin_PROGRAMS = lctl wiretest l_getidentity llverfs llverdev \
	llog_reader lr_reader lshowmount lustre_rsync \
	ll_recover_lost_found_objs ltrack_stats ll_decode_filter_fid
if SERVER
sbin_PROGRAMS += mkfs.lustre tunefs.lustre
endif
if LIBPTHREAD
sbin_PROGRAMS += loadgen
endif
bin_PROGRAMS = lfs req_layout
bin_SCRIPTS = $(bin_scripts)
sbin_SCRIPTS = $(sbin_scripts)
endif # UTILS

lib_LIBRARIES = liblustreapi.a
if LDISKFS_ENABLED
lib_LIBRARIES += libiam.a
endif
noinst_LIBRARIES = liblustreapitmp.a

lctl_SOURCES = lustre_lfsck.c obd.c lustre_cfg.c lctl.c obdctl.h
lctl_LDADD := $(LIBREADLINE) liblustreapi.a $(LIBPTLCTL) $(PTHREAD_LIBS)
lctl_DEPENDENCIES := $(LIBPTLCTL) liblustreapi.a

lfs_SOURCES = lfs.c obd.c lustre_cfg.c
lfs_LDADD := $(LIBREADLINE) liblustreapi.a $(LIBPTLCTL) $(PTHREAD_LIBS)
lfs_DEPENDENCIES := $(LIBPTLCTL) liblustreapi.a

loadgen_SOURCES = loadgen.c lustre_cfg.c obd.c
loadgen_LDADD := $(LIBREADLINE) liblustreapi.a $(LIBPTLCTL) $(PTHREAD_LIBS)
loadgen_DEPENDENCIES := $(LIBPTLCTL) liblustreapi.a

lustre_rsync_SOURCES = lustre_rsync.c obd.c lustre_cfg.c lustre_rsync.h
lustre_rsync_LDADD := $(LIBREADLINE) liblustreapi.a $(LIBPTLCTL) $(PTHREAD_LIBS)
lustre_rsync_DEPENDENCIES := $(LIBPTLCTL) liblustreapi.a

ll_recover_lost_found_objs_SOURCES = ll_recover_lost_found_objs.c
ll_recover_lost_found_objs_LDADD := $(LIBPTLCTL)
ll_recover_lost_found_objs_DEPENDENCIES := $(LIBPTLCTL)

lshowmount_SOURCES = lshowmount.c nidlist.c nidlist.h

if EXT2FS_DEVEL
EXT2FSLIB = -lext2fs
E2PLIB = -le2p
else
E2PLIB =
EXT2FSLIB =
endif

if BLKID
BLKIDLIB = -lblkid
else
BLKIDLIB =
endif

llverfs_LDADD := $(EXT2FSLIB) $(E2PLIB)

llverdev_LDADD := $(EXT2FSLIB) $(BLKIDLIB)

L_IOCTL := $(top_builddir)/libcfs/libcfs/util/l_ioctl.c
L_KERNELCOMM := $(top_builddir)/libcfs/libcfs/kernel_user_comm.c
liblustreapitmp_a_SOURCES = liblustreapi.c liblustreapi_hsm.c \
			    lustreapi_internal.h \
			    $(L_IOCTL) $(L_KERNELCOMM)

# build static and shared lib lustreapi
liblustreapi.a : liblustreapitmp.a
	rm -f liblustreapi.a liblustreapi.so
	$(CC) $(LDFLAGS) -shared -o liblustreapi.so `$(AR) -t liblustreapitmp.a`
	mv liblustreapitmp.a liblustreapi.a

install-exec-hook: liblustreapi.so
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	@list=$< ; for p in $$list; do \
	  if test -f $$p; then \
	    f="`echo $$p | sed -e 's|^.*/||'`"; \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(libdir)/$$f"; \
	    $(INSTALL_DATA) $$p $(DESTDIR)$(libdir)/$$f; \
	  else :; fi; \
	done

if LDISKFS_ENABLED
libiam_a_SOURCES = libiam.c
endif

wirecheck_SOURCES = wirecheck.c
wirecheck_CPPFLAGS = -DCC="\"$(CC)\""

wiretest_SOURCES = wiretest.c

obdio_SOURCES = obdio.c obdiolib.c obdiolib.h
obdbarrier_SOURCES = obdbarrier.c obdiolib.c obdiolib.h
req_layout_SOURCES = req-layout.c

llog_reader_SOURCES = llog_reader.c
llog_reader_LDADD := $(LIBPTLCTL)
llog_reader_DEPENDENCIES := $(LIBPTLCTL)

lr_reader_SOURCES = lr_reader.c

mount_lustre_SOURCES = mount_lustre.c mount_utils.c mount_utils.h
mount_lustre_LDADD := $(LIBPTLCTL)
mount_lustre_DEPENDENCIES := $(LIBPTLCTL)
if LDISKFS_ENABLED
mount_lustre_SOURCES += mount_utils_ldiskfs.c
endif
if ZFS_ENABLED
mount_lustre_SOURCES += mount_utils_zfs.c
mount_lustre_CPPFLAGS = -DHAVE_IOCTL_IN_UNISTD_H
mount_lustre_CPPFLAGS += $(EXTRA_LIBZFS_INCLUDE)
mount_lustre_LDFLAGS = -pthread -rdynamic -ldl
endif

mkfs_lustre_SOURCES = mkfs_lustre.c mount_utils.c mount_utils.h
mkfs_lustre_CPPFLAGS = -UTUNEFS $(AM_CPPFLAGS)
mkfs_lustre_LDADD := $(LIBPTLCTL)
mkfs_lustre_DEPENDENCIES := $(LIBPTLCTL)
if LDISKFS_ENABLED
mkfs_lustre_SOURCES += mount_utils_ldiskfs.c
mkfs_lustre_LDADD += libiam.a
mkfs_lustre_DEPENDENCIES += $(LIBPTLCTL)
endif
if ZFS_ENABLED
mkfs_lustre_SOURCES += mount_utils_zfs.c
mkfs_lustre_CPPFLAGS += -DHAVE_IOCTL_IN_UNISTD_H
mkfs_lustre_CPPFLAGS += $(EXTRA_LIBZFS_INCLUDE)
mkfs_lustre_LDFLAGS = -pthread -rdynamic -ldl
endif

tunefs_lustre_SOURCES = mkfs_lustre.c mount_utils.c mount_utils.h
tunefs_lustre_CPPFLAGS = -DTUNEFS $(AM_CPPFLAGS)
tunefs_lustre_LDADD := $(mkfs_lustre_LDADD)
tunefs_lustre_DEPENDENCIES := $(mkfs_lustre_DEPENDENCIES)
if LDISKFS_ENABLED
tunefs_lustre_SOURCES += mount_utils_ldiskfs.c
endif
if ZFS_ENABLED
tunefs_lustre_SOURCES += mount_utils_zfs.c
tunefs_lustre_CPPFLAGS += -DHAVE_IOCTL_IN_UNISTD_H
tunefs_lustre_CPPFLAGS += $(EXTRA_LIBZFS_INCLUDE)
tunefs_lustre_LDFLAGS = -pthread -rdynamic -ldl
endif

l_getidentity_SOURCES = l_getidentity.c
l_getidentity_LDADD := $(LIBPTLCTL)
l_getidentity_DEPENDENCIES := $(LIBPTLCTL)

ltrack_stats_SOURCES = ltrack_stats.c

EXTRA_DIST = $(sbin_scripts) $(bin_scripts)

# NOTE: this should only be run on i386.
newwiretest: wirehdr.c wirecheck
	cp wirehdr.c wiretest.c
	LANG=C ./wirecheck >> wiretest.c
	cp ../ptlrpc/wirehdr.c ../ptlrpc/wiretest.c
	LANG=C ./wirecheck >> ../ptlrpc/wiretest.c

