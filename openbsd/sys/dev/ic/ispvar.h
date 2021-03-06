/*	$OpenBSD: ispvar.h,v 1.6 1999/04/04 02:07:27 mjacob Exp $ */
/* release_03_25_99 */
/*
 * Soft Definitions for for Qlogic ISP SCSI adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_ISPVAR_H
#define	_ISPVAR_H

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/ispmbox.h>
#endif
#ifdef	__FreeBSD__
#include <dev/isp/ispmbox.h>
#endif
#ifdef	__linux__
#include "ispmbox.h"
#endif

#define	ISP_CORE_VERSION_MAJOR	1
#define	ISP_CORE_VERSION_MINOR	7

/*
 * Vector for bus specific code to provide specific services.
 */
struct ispsoftc;
struct ispmdvec {
	u_int16_t	(*dv_rd_reg) __P((struct ispsoftc *, int));
	void		(*dv_wr_reg) __P((struct ispsoftc *, int, u_int16_t));
	int		(*dv_mbxdma) __P((struct ispsoftc *));
	int		(*dv_dmaset) __P((struct ispsoftc *,
		ISP_SCSI_XFER_T *, ispreq_t *, u_int8_t *, u_int8_t));
	void		(*dv_dmaclr)
		__P((struct ispsoftc *, ISP_SCSI_XFER_T *, u_int32_t));
	void		(*dv_reset0) __P((struct ispsoftc *));
	void		(*dv_reset1) __P((struct ispsoftc *));
	void		(*dv_dregs) __P((struct ispsoftc *));
	const u_int16_t *dv_ispfw;	/* ptr to f/w */
	u_int16_t 	dv_fwlen;	/* length of f/w */
	u_int16_t	dv_codeorg;	/* code ORG for f/w */
	u_int16_t	dv_fwrev;	/* f/w revision */
	/*
	 * Initial values for conf1 register
	 */
	u_int16_t	dv_conf1;
	u_int16_t	dv_clock;	/* clock frequency */
};

#define	MAX_TARGETS	16
#ifdef	ISP2100_FABRIC
#define	MAX_FC_TARG	256
#else
#define	MAX_FC_TARG	126
#endif
#define	DEFAULT_LOOPID	113

/* queue length must be a power of two */
#define	QENTRY_LEN			64
#define	RQUEST_QUEUE_LEN		MAXISPREQUEST
#define	RESULT_QUEUE_LEN		(MAXISPREQUEST/2)
#define	ISP_QUEUE_ENTRY(q, idx)		((q) + ((idx) * QENTRY_LEN))
#define	ISP_QUEUE_SIZE(n)		((n) * QENTRY_LEN)
#define	ISP_NXT_QENTRY(idx, qlen)	(((idx) + 1) & ((qlen)-1))
#define ISP_QAVAIL(in, out, qlen)	\
	((in == out)? (qlen - 1) : ((in > out)? \
		((qlen - 1) - (in - out)) : (out - in - 1)))
/*
 * SCSI Specific Host Adapter Parameters
 */

typedef struct {
        u_int		isp_req_ack_active_neg	: 1,	
	        	isp_data_line_active_neg: 1,
			isp_cmd_dma_burst_enable: 1,
			isp_data_dma_burst_enabl: 1,
			isp_fifo_threshold	: 3,
			isp_ultramode		: 1,
			isp_diffmode		: 1,
			isp_lvdmode		: 1,
			isp_fast_mttr		: 1,
			isp_initiator_id	: 4,
        		isp_async_data_setup	: 4;
        u_int16_t	isp_selection_timeout;
        u_int16_t	isp_max_queue_depth;
	u_int16_t	isp_clock;
	u_int8_t	isp_tag_aging;
       	u_int8_t	isp_bus_reset_delay;
        u_int8_t	isp_retry_count;
        u_int8_t	isp_retry_delay;
	struct {
		u_int
			dev_enable	:	1,
			dev_announced	:	1,
			dev_update	:	1,
			dev_refresh	:	1,
			exc_throttle	:	8,
			cur_offset	:	4,
			sync_offset	:	4;
		u_int8_t	cur_period;	/* current sync period */
		u_int8_t	sync_period;	/* goal sync period */
		u_int16_t	dev_flags;	/* goal device flags */
		u_int16_t	cur_dflags;	/* current device flags */
	} isp_devparam[MAX_TARGETS];
} sdparam;	/* scsi device parameters */

/*
 * Device Flags
 */
#define	DPARM_DISC	0x8000
#define	DPARM_PARITY	0x4000
#define	DPARM_WIDE	0x2000
#define	DPARM_SYNC	0x1000
#define	DPARM_TQING	0x0800
#define	DPARM_ARQ	0x0400
#define	DPARM_QFRZ	0x0200
#define	DPARM_RENEG	0x0100
#define	DPARM_NARROW	0x0080	/* Possibly only available with >= 7.55 fw */
#define	DPARM_ASYNC	0x0040	/* Possibly only available with >= 7.55 fw */
#define	DPARM_DEFAULT	(0xFF00 & ~DPARM_QFRZ)
#define	DPARM_SAFE_DFLT	(DPARM_DEFAULT & ~(DPARM_WIDE|DPARM_SYNC|DPARM_TQING))


/* technically, not really correct, as they need to be rated based upon clock */
#define	ISP_40M_SYNCPARMS	0x080a
#define ISP_20M_SYNCPARMS	0x080c
#define ISP_10M_SYNCPARMS	0x0c19
#define ISP_08M_SYNCPARMS	0x0c25
#define ISP_05M_SYNCPARMS	0x0c32
#define ISP_04M_SYNCPARMS	0x0c41

/*
 * Fibre Channel Specifics
 */
typedef struct {
	u_int64_t		isp_wwn;	/* WWN of adapter */
	u_int8_t		isp_loopid;	/* hard loop id */
	u_int8_t		isp_alpa;	/* ALPA */
	u_int8_t		isp_execthrottle;
        u_int8_t		isp_retry_delay;
        u_int8_t		isp_retry_count;
	u_int8_t		isp_fwstate;	/* ISP F/W state */
	u_int16_t		isp_maxalloc;
	u_int16_t		isp_maxfrmlen;
	u_int16_t		isp_fwoptions;
	/*
	 * Port Data Base
	 */
	isp_pdb_t		isp_pdb[MAX_FC_TARG];

	/*
	 * Scratch DMA mapped in area to fetch Port Database stuff, etc.
	 */
	volatile caddr_t	isp_scratch;
	u_int32_t		isp_scdma;
} fcparam;

#define	ISP2100_SCRLEN		0x100

#define	FW_CONFIG_WAIT		0x0000
#define	FW_WAIT_AL_PA		0x0001
#define	FW_WAIT_LOGIN		0x0002
#define	FW_READY		0x0003
#define	FW_LOSS_OF_SYNC		0x0004
#define	FW_ERROR		0x0005
#define	FW_REINIT		0x0006
#define	FW_NON_PART		0x0007

#ifdef	ISP_TARGET_MODE
/*
 * Some temporary Target Mode definitions
 */
typedef struct tmd_cmd {
	u_int8_t	cd_iid;		/* initiator */
	u_int8_t	cd_tgt;		/* target */
	u_int8_t	cd_lun;		/* LUN for this command */
	u_int8_t	cd_state;
	u_int8_t	cd_cdb[16];	/* command bytes */
	u_int8_t	cd_sensedata[20];
	u_int16_t	cd_rxid;
	u_int32_t	cd_datalen;
	u_int32_t	cd_totbytes;
	void *		cd_hba;
} tmd_cmd_t;

/*
 * Async Target Mode Event Definitions
 */
#define	TMD_BUS_RESET	0
#define	TMD_BDR		1

/*
 * Immediate Notify data structure.
 */
#define NOTIFY_MSGLEN	5
typedef struct {
	u_int8_t	nt_iid;			/* initiator */
	u_int8_t	nt_tgt;			/* target */
	u_int8_t	nt_lun;			/* LUN for this command */
	u_int8_t	nt_msg[NOTIFY_MSGLEN];	/* SCSI message byte(s) */
} tmd_notify_t;

#endif

/*
 * Soft Structure per host adapter
 */
struct ispsoftc {
	/*
	 * Platform (OS) specific data
	 */
	struct isposinfo	isp_osinfo;

	/*
	 * Pointer to bus specific data
	 */
	struct ispmdvec *	isp_mdvec;

	/*
	 * Mostly nonvolatile state, debugging, etc..
	 */

	u_int				: 8,
			isp_confopts	: 8,
			isp_port	: 1,	/* for dual ported impls */
			isp_used	: 1,
			isp_dblev	: 3,
			isp_gotdparms	: 1,
			isp_dogactive	: 1,
			isp_bustype	: 1,	/* BUS Implementation */
			isp_type	: 8;	/* HBA Type and Revision */

	u_int16_t		isp_fwrev;	/* Running F/W revision */
	u_int16_t		isp_romfw_rev;	/* 'ROM' F/W revision */
	void * 			isp_param;

	/*
	 * Volatile state
	 */

	volatile u_int
				:	19,
		isp_state	:	3,
		isp_sendmarker	:	1,	/* send a marker entry */
		isp_update	:	1,	/* update parameters */
		isp_nactive	:	9;	/* how many commands active */

	/*
	 * Result and Request Queue indices.
	 */
	volatile u_int8_t	isp_reqodx;	/* index of last ISP pickup */
	volatile u_int8_t	isp_reqidx;	/* index of next request */
	volatile u_int8_t	isp_residx;	/* index of next result */
	volatile u_int8_t	isp_seqno;	/* rolling sequence # */

	/*
	 * Sheer laziness, but it gets us around the problem
	 * where we don't have a clean way of remembering
	 * which transaction is bound to which ISP queue entry.
	 *
	 * There are other more clever ways to do this, but,
	 * jeez, so I blow a couple of KB per host adapter...
	 * and it *is* faster.
	 */
	ISP_SCSI_XFER_T *isp_xflist[RQUEST_QUEUE_LEN];

	/*
	 * request/result queues and dma handles for them.
	 */
	volatile caddr_t	isp_rquest;
	volatile caddr_t	isp_result;
	u_int32_t		isp_rquest_dma;
	u_int32_t		isp_result_dma;

#ifdef	ISP_TARGET_MODE
	/*
	 * Vectors for handling target mode support.
	 *
	 * isp_tmd_newcmd is for feeding a newly arrived command to some
	 * upper layer.
	 *
	 * isp_tmd_event is for notifying some upper layer that an event has
	 * occurred that is not necessarily tied to any target (e.g., a SCSI
	 * Bus Reset).
	 *
	 * isp_tmd_notify is for notifying some upper layer that some
	 * event is now occurring that is either pertinent for a specific
	 * device or for a specific command (e.g., BDR or ABORT TAG).
	 *
	 * It is left undefined (for now) how pools of commands are managed.
	 */
	void		(*isp_tmd_newcmd) __P((void *, tmd_cmd_t *));
	void		(*isp_tmd_event) __P((void *, int));
	void		(*isp_tmd_notify) __P((void *, tmd_notify_t *));
#endif	   
};

/*
 * ISP States
 */
#define	ISP_NILSTATE	0
#define	ISP_RESETSTATE	1
#define	ISP_INITSTATE	2
#define	ISP_RUNSTATE	3

/*
 * ISP Configuration Options
 */
#define	ISP_CFG_NORELOAD	0x80	/* don't download f/w */
#define	ISP_CFG_NONVRAM		0x40	/* ignore NVRAM */

#define	ISP_FW_REV(maj, min)	((maj) << 10| (min))

/*
 * Bus (implementation) types
 */
#define	ISP_BT_PCI		0	/* PCI Implementations */
#define	ISP_BT_SBUS		1	/* SBus Implementations */

/*
 * Chip Types
 */
#define	ISP_HA_SCSI		0xf
#define	ISP_HA_SCSI_UNKNOWN	0x1
#define	ISP_HA_SCSI_1020	0x2
#define	ISP_HA_SCSI_1020A	0x3
#define	ISP_HA_SCSI_1040	0x4
#define	ISP_HA_SCSI_1040A	0x5
#define	ISP_HA_SCSI_1040B	0x6
#define	ISP_HA_SCSI_1080	0xd
#define	ISP_HA_SCSI_12X0	0xe
#define	ISP_HA_FC		0xf0
#define	ISP_HA_FC_2100		0x10

#define	IS_SCSI(isp)	(isp->isp_type & ISP_HA_SCSI)
#define	IS_1080(isp)	(isp->isp_type == ISP_HA_SCSI_1080)
#define	IS_12X0(isp)	(isp->isp_type == ISP_HA_SCSI_12X0)
#define	IS_FC(isp)	(isp->isp_type & ISP_HA_FC)

/*
 * Macros to read, write ISP registers through bus specific code.
 */

#define	ISP_READ(isp, reg)	\
	(*(isp)->isp_mdvec->dv_rd_reg)((isp), (reg))

#define	ISP_WRITE(isp, reg, val)	\
	(*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), (val))

#define	ISP_MBOXDMASETUP(isp)	\
	(*(isp)->isp_mdvec->dv_mbxdma)((isp))

#define	ISP_DMASETUP(isp, xs, req, iptrp, optr)	\
	(*(isp)->isp_mdvec->dv_dmaset)((isp), (xs), (req), (iptrp), (optr))

#define	ISP_DMAFREE(isp, xs, seqno)	\
	if ((isp)->isp_mdvec->dv_dmaclr) \
		 (*(isp)->isp_mdvec->dv_dmaclr)((isp), (xs), (seqno))

#define	ISP_RESET0(isp)	\
	if ((isp)->isp_mdvec->dv_reset0) (*(isp)->isp_mdvec->dv_reset0)((isp))
#define	ISP_RESET1(isp)	\
	if ((isp)->isp_mdvec->dv_reset1) (*(isp)->isp_mdvec->dv_reset1)((isp))
#define	ISP_DUMPREGS(isp)	\
	if ((isp)->isp_mdvec->dv_dregs) (*(isp)->isp_mdvec->dv_dregs)((isp))

#define	ISP_SETBITS(isp, reg, val)	\
 (*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), ISP_READ((isp), (reg)) | (val))

#define	ISP_CLRBITS(isp, reg, val)	\
 (*(isp)->isp_mdvec->dv_wr_reg)((isp), (reg), ISP_READ((isp), (reg)) & ~(val))

/*
 * Function Prototypes
 */

/*
 * Reset Hardware. Totally. Assumes that you'll follow this with
 * a call to isp_init.
 */
void isp_reset __P((struct ispsoftc *));

/*
 * Initialize Hardware to known state
 */
void isp_init __P((struct ispsoftc *));

/*
 * Reset the ISP and call completion for any orphaned commands.
 */
void isp_restart __P((struct ispsoftc *));

/*
 * Interrupt Service Routine
 */
int isp_intr __P((void *));

/*
 * Command Entry Point
 */
int32_t ispscsicmd __P((ISP_SCSI_XFER_T *));

/*
 * Platform Dependent to External to Internal Control Function
 *
 * For: 	Aborting a running command	- arg is an ISP_SCSI_XFER_T *
 *		Resetting a Device		- arg is target to reset
 *		Resetting a BUS			- arg is ignored
 *		Updating parameters		- arg is ignored
 *
 * First argument is this instance's softc pointer.
 * Second argument is an index into xflist array.
 * Assumes all locks must be held already.
 */
typedef enum {
	ISPCTL_RESET_BUS,
	ISPCTL_RESET_DEV,
	ISPCTL_ABORT_CMD,
	ISPCTL_UPDATE_PARAMS,
	ISPCTL_FCLINK_TEST
} ispctl_t;
int isp_control __P((struct ispsoftc *, ispctl_t, void *));


/*
 * Platform Dependent to Internal to External Control Function
 * (each platform must provide such a function)
 *
 * For: 	Announcing Target Paramter Changes (arg is target)
 *
 * Assumes all locks are held.
 */

typedef enum {
	ISPASYNC_NEW_TGT_PARAMS,
	ISPASYNC_BUS_RESET,		/* Bus Reset */
	ISPASYNC_LOOP_DOWN,		/* Obvious FC only */
	ISPASYNC_LOOP_UP,		/* "" */
	ISPASYNC_PDB_CHANGE_COMPLETE,	/* "" */
	ISPASYNC_CHANGE_NOTIFY		/* "" */
} ispasync_t;
int isp_async __P((struct ispsoftc *, ispasync_t, void *));

/*
 * lost command routine (XXXX IN TRANSITION XXXX)
 */
void isp_lostcmd __P((struct ispsoftc *, ISP_SCSI_XFER_T *));


#endif	/* _ISPVAR_H */
