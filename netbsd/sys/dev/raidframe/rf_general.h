/*	$NetBSD: rf_general.h,v 1.3 1999/02/05 00:06:12 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * rf_general.h -- some general-use definitions
 */

/*#define NOASSERT*/

#ifndef _RF__RF_GENERAL_H_
#define _RF__RF_GENERAL_H_

/* error reporting and handling */

#ifdef _KERNEL
#include<sys/systm.h>		/* printf, sprintf, and friends */
#endif

#define RF_ERRORMSG(s)            printf((s))
#define RF_ERRORMSG1(s,a)         printf((s),(a))
#define RF_ERRORMSG2(s,a,b)       printf((s),(a),(b))
#define RF_ERRORMSG3(s,a,b,c)     printf((s),(a),(b),(c))
#define RF_ERRORMSG4(s,a,b,c,d)   printf((s),(a),(b),(c),(d))
#define RF_ERRORMSG5(s,a,b,c,d,e) printf((s),(a),(b),(c),(d),(e))
#define perror(x)
extern char rf_panicbuf[];
#define RF_PANIC() {sprintf(rf_panicbuf,"raidframe error at line %d file %s",__LINE__,__FILE__); panic(rf_panicbuf);}

#ifdef _KERNEL
#ifdef RF_ASSERT
#undef RF_ASSERT
#endif				/* RF_ASSERT */
#ifndef NOASSERT
#define RF_ASSERT(_x_) { \
  if (!(_x_)) { \
    sprintf(rf_panicbuf, \
        "raidframe error at line %d file %s (failed asserting %s)\n", \
        __LINE__, __FILE__, #_x_); \
    panic(rf_panicbuf); \
  } \
}
#else				/* !NOASSERT */
#define RF_ASSERT(x) {/*noop*/}
#endif				/* !NOASSERT */
#else				/* _KERNEL */
#define RF_ASSERT(x) {/*noop*/}
#endif				/* _KERNEL */

/* random stuff */
#define RF_MAX(a,b) (((a) > (b)) ? (a) : (b))
#define RF_MIN(a,b) (((a) < (b)) ? (a) : (b))

/* divide-by-zero check */
#define RF_DB0_CHECK(a,b) ( ((b)==0) ? 0 : (a)/(b) )

/* get time of day */
#define RF_GETTIME(_t) microtime(&(_t))

/*
 * zero memory- not all bzero calls go through here, only
 * those which in the kernel may have a user address
 */

#define RF_BZERO(_bp,_b,_l)  bzero(_b,_l)	/* XXX This is likely
						 * incorrect. GO */


#define RF_UL(x)           ((unsigned long) (x))
#define RF_PGMASK          RF_UL(NBPG-1)
#define RF_BLIP(x)         (NBPG - (RF_UL(x) & RF_PGMASK))	/* bytes left in page */
#define RF_PAGE_ALIGNED(x) ((RF_UL(x) & RF_PGMASK) == 0)

#if DKUSAGE > 0
#define RF_DKU_END_IO(_unit_,_bp_) { \
	int s = splbio(); \
	dku_end_io(DKU_RAIDFRAME_BUS, _unit_, 0, \
			(((_bp_)->b_flags&(B_READ|B_WRITE) == B_READ) ? \
		    CAM_DIR_IN : CAM_DIR_OUT), \
			(_bp_)->b_bcount); \
	splx(s); \
}
#else				/* DKUSAGE > 0 */
#define RF_DKU_END_IO(unit) { /* noop */ }
#endif				/* DKUSAGE > 0 */

#ifdef __STDC__
#define RF_STRING(_str_) #_str_
#else				/* __STDC__ */
#define RF_STRING(_str_) "_str_"
#endif				/* __STDC__ */

#endif				/* !_RF__RF_GENERAL_H_ */
