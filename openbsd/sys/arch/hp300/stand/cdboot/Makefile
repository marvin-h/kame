#	$OpenBSD: Makefile,v 1.3 1999/10/05 13:05:46 downsj Exp $

PROG=	cdboot

NOMAN=

.PATH:	${.CURDIR}/../uboot

CFLAGS= -O2 -DOSREV=\"${OSREV}\" -msoft-float -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

SRCS=	srt0.S cdboot.c clock.c conf.c cons.c devopen.c

LIBS=	${LIBCOMMON} ${LIBSA} ${LIBZ} ${LIBKERN}

${OBJS}:	${.CURDIR}/../Makefile.inc

CLEANFILES+=	cdboot.lif

cdboot.lif:	cdboot
	${MKBOOT_PROG} cdboot $@

cdboot:	${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e begin ${OBJS} ${LIBS} -o $@
	@size $@
	@echo $@ total size may not exceed 30000 bytes

install:	cdboot.lif ${MKBOOT_PROG}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 cdboot.lif \
	    ${DESTDIR}/usr/mdec

cdboot.o:	Makefile

.include <bsd.prog.mk>
