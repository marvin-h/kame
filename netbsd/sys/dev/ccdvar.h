/*	$NetBSD: ccdvar.h,v 1.17 1999/01/21 00:35:16 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: cdvar.h 1.1 90/07/09$
 *
 *	@(#)cdvar.h	8.1 (Berkeley) 6/10/93
 */

#include <sys/lock.h>
#include <sys/pool.h>
#include <sys/queue.h>

/*
 * Dynamic configuration and disklabel support by:
 *	Jason R. Thorpe <thorpej@nas.nasa.gov>
 *	Numerical Aerodynamic Simulation Facility
 *	Mail Stop 258-6
 *	NASA Ames Research Center
 *	Moffett Field, CA 94035
 */

/*
 * This structure is used to configure a ccd via ioctl(2).
 */
struct ccd_ioctl {
	char	**ccio_disks;		/* pointer to component paths */
	int	ccio_ndisks;		/* number of disks to concatenate */
	int	ccio_ileave;		/* interleave (DEV_BSIZE blocks) */
	int	ccio_flags;		/* see sc_flags below */
	int	ccio_unit;		/* unit number: use varies */
	size_t	ccio_size;		/* (returned) size of ccd */
};

/*
 * Component info table.
 * Describes a single component of a concatenated disk.
 */
struct ccdcinfo {
	struct vnode	*ci_vp;			/* device's vnode */
	dev_t		ci_dev;			/* XXX: device's dev_t */
	size_t		ci_size; 		/* size */
	char		*ci_path;		/* path to component */
	size_t		ci_pathlen;		/* length of component path */
};

/*
 * Interleave description table.
 * Computed at boot time to speed irregular-interleave lookups.
 * The idea is that we interleave in "groups".  First we interleave
 * evenly over all component disks up to the size of the smallest
 * component (the first group), then we interleave evenly over all
 * remaining disks up to the size of the next-smallest (second group),
 * and so on.
 *
 * Each table entry describes the interleave characteristics of one
 * of these groups.  For example if a concatenated disk consisted of
 * three components of 5, 3, and 7 DEV_BSIZE blocks interleaved at
 * DEV_BSIZE (1), the table would have three entries:
 *
 *	ndisk	startblk	startoff	dev
 *	3	0		0		0, 1, 2
 *	2	9		3		0, 2
 *	1	13		5		2
 *	0	-		-		-
 *
 * which says that the first nine blocks (0-8) are interleaved over
 * 3 disks (0, 1, 2) starting at block offset 0 on any component disk,
 * the next 4 blocks (9-12) are interleaved over 2 disks (0, 2) starting
 * at component block 3, and the remaining blocks (13-14) are on disk
 * 2 starting at offset 5.
 */
struct ccdiinfo {
	int	ii_ndisk;	/* # of disks range is interleaved over */
	daddr_t	ii_startblk;	/* starting scaled block # for range */
	daddr_t	ii_startoff;	/* starting component offset (block #) */
	int	*ii_index;	/* ordered list of components in range */
};

/*
 * Concatenated disk pseudo-geometry information.
 */
struct ccdgeom {
	u_int32_t	ccg_secsize;	/* # bytes per sector */
	u_int32_t	ccg_nsectors;	/* # data sectors per track */
	u_int32_t	ccg_ntracks;	/* # tracks per cylinder */
	u_int32_t	ccg_ncylinders;	/* # cylinders per unit */
};

struct ccdbuf;

/*
 * A concatenated disk is described after initialization by this structure.
 */
struct ccd_softc {
	int		 sc_flags;		/* flags */
	size_t		 sc_size;		/* size of ccd */
	int		 sc_ileave;		/* interleave */
	int		 sc_nccdisks;		/* number of components */
	struct ccdcinfo	 *sc_cinfo;		/* component info */
	struct ccdiinfo	 *sc_itable;		/* interleave table */
	struct ccdgeom   sc_geom;		/* pseudo geometry info */
	char		 sc_xname[8];		/* XXX external name */
	struct disk	 sc_dkdev;		/* generic disk device info */
	struct pool	 sc_cbufpool;		/* component buffer pool */
	struct lock	 sc_lock;		/* lock on this structure */
};

/* sc_flags */
#define	CCDF_SWAP	0x01	/* interleave should be dmmax */
#define	CCDF_UNIFORM	0x02	/* use LCCD of sizes for uniform interleave */
#define CCDF_INITED	0x10	/* unit has been initialized */
#define CCDF_WLABEL	0x20	/* label area is writable */
#define CCDF_LABELLING	0x40	/* unit is currently being labelled */

/* Mask of user-settable ccd flags. */
#define CCDF_USERMASK	(CCDF_SWAP|CCDF_UNIFORM)

/*
 * Before you can use a unit, it must be configured with CCDIOCSET.
 * The configuration persists across opens and closes of the device;
 * a CCDIOCCLR must be used to reset a configuration.  An attempt to
 * CCDIOCSET an already active unit will return EBUSY.  Attempts to
 * CCDIOCCLR an inactive unit will return ENXIO.
 */
#define CCDIOCSET	_IOWR('F', 16, struct ccd_ioctl)   /* enable ccd */
#define CCDIOCCLR	_IOW('F', 17, struct ccd_ioctl)    /* disable ccd */
