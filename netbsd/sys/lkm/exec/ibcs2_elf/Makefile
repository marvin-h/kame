#	$NetBSD: Makefile,v 1.3 2001/12/12 11:49:06 lukem Exp $

.include "../Makefile.inc"

.PATH:	$S/compat/ibcs2

CPPFLAGS+=	-nostdinc -I$S -D_KERNEL -DEXEC_ELF32

KMOD=	exec_ibcs2_elf

SRCS=	lkminit_exec.c
SRCS+=	ibcs2_exec_elf32.c

.include <bsd.kmod.mk>
