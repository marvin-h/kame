# $FreeBSD: src/sys/modules/vinum/Makefile,v 1.17 2000/01/04 23:46:41 grog Exp $ 

MAINTAINER=  grog@FreeBSD.org

.PATH: ${.CURDIR}/../../dev/vinum
KMOD= vinum

SRCS= opt_vinum.h request.h vinum.c vinumconfig.c vinumdaemon.c \
	vinumext.h vinumhdr.h vinuminterrupt.c vinumio.c vinumio.h \
	vinumioctl.c vinumkw.h vinumlock.c vinummemory.c vinumparser.c \
	vinumraid5.c vinumrequest.c vinumrevive.c vinumstate.c \
	vinumstate.h vinumutil.c vinumvar.h vnode_if.h

NOMAN=

# If you remove -DVINUMDEBUG here, you must also do it in src/sbin/Makefile.
# Otherwise a number of ioctls will fail.
CFLAGS+=	-DVINUMDEBUG -g -O

.include <bsd.kmod.mk>
