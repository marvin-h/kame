# $FreeBSD: src/sys/modules/coda/Makefile,v 1.12 2002/01/11 15:48:38 ru Exp $

.PATH: ${.CURDIR}/../../coda

KMOD=	coda
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c vcoda.h

CLEANFILES=	vcoda.h

vcoda.h:
	echo "#define NVCODA 4" > vcoda.h

.include <bsd.kmod.mk>
