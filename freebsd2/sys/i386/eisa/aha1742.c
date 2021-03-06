/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 *
 *      $Id: aha1742.c,v 1.55.2.1 1998/05/06 18:58:43 gibbs Exp $
 */

#include <sys/types.h>

#ifdef	KERNEL			/* don't laugh, it compiles as a program too.. look */
#include "opt_ddb.h"
#include "ahb.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <i386/eisa/eisaconf.h>
#else
#define	NAHB	1
#endif /*KERNEL */

/* */

#ifdef KERNEL
# ifdef DDB
#define	fatal_if_no_DDB()
# else
#define	fatal_if_no_DDB() panic("panic for historical reasons")
# endif
#endif

typedef unsigned long int physaddr;
#include <sys/kernel.h>

#define KVTOPHYS(x)   vtophys(x)

#define AHB_ECB_MAX	32	/* store up to 32ECBs at any one time     */
				/* in aha1742 H/W ( Not MAX ? )         */
#define	ECB_HASH_SIZE	32	/* when we have a physical addr. for      */
				/* a ecb and need to find the ecb in    */
				/* space, look it up in the hash table  */
#define	ECB_HASH_SHIFT	9	/* only hash on multiples of 512  */
#define ECB_HASH(x)	((((long int)(x))>>ECB_HASH_SHIFT) % ECB_HASH_SIZE)

#define	AHB_NSEG	33	/* number of dma segments supported       */

#define EISA_DEVICE_ID_ADAPTEC_1740  0x04900000
#define	AHB_EISA_IOSIZE 0x100
#define	AHB_EISA_SLOT_OFFSET 0xc00

/* AHA1740 EISA board control registers (Offset from slot base) */
#define	EBCTRL		0x084
#define  CDEN		0x01
/*
 * AHA1740 EISA board mode registers (Offset from slot base)
 */
#define PORTADDR	0x0C0
#define	 PORTADDR_ENHANCED	0x80
#define BIOSADDR	0x0C1
#define	INTDEF		0x0C2
#define	SCSIDEF		0x0C3
#define	BUSDEF		0x0C4
#define	RESV0		0x0C5
#define	RESV1		0x0C6
#define	RESV2		0x0C7
/**** bit definitions for INTDEF ****/
#define	INT9	0x00
#define	INT10	0x01
#define	INT11	0x02
#define	INT12	0x03
#define	INT14	0x05
#define	INT15	0x06
#define INTHIGH 0x08		/* int high=ACTIVE (else edge) */
#define	INTEN	0x10
/**** bit definitions for SCSIDEF ****/
#define	HSCSIID	0x0F		/* our SCSI ID */
#define	RSTPWR	0x10		/* reset scsi bus on power up or reset */
/**** bit definitions for BUSDEF ****/
#define	B0uS	0x00		/* give up bus immediatly */
#define	B4uS	0x01		/* delay 4uSec. */
#define	B8uS	0x02
/*
 * AHA1740 ENHANCED mode mailbox control regs (Offset from slot base)
 */
#define MBOXOUT0	0x0D0
#define MBOXOUT1	0x0D1
#define MBOXOUT2	0x0D2
#define MBOXOUT3	0x0D3

#define	ATTN		0x0D4
#define	G2CNTRL		0x0D5
#define	G2INTST		0x0D6
#define G2STAT		0x0D7

#define	MBOXIN0		0x0D8
#define	MBOXIN1		0x0D9
#define	MBOXIN2		0x0DA
#define	MBOXIN3		0x0DB

#define G2STAT2		0x0DC

/*
 * Bit definitions for the 5 control/status registers
 */
#define	ATTN_TARGET		0x0F
#define	ATTN_OPCODE		0xF0
#define  OP_IMMED		0x10
#define	  AHB_TARG_RESET	0x80
#define  OP_START_ECB		0x40
#define  OP_ABORT_ECB		0x50

#define	G2CNTRL_SET_HOST_READY	0x20
#define	G2CNTRL_CLEAR_EISA_INT	0x40
#define	G2CNTRL_HARD_RESET	0x80

#define	G2INTST_TARGET		0x0F
#define	G2INTST_INT_STAT	0xF0
#define	 AHB_ECB_OK		0x10
#define	 AHB_ECB_RECOVERED	0x50
#define	 AHB_HW_ERR		0x70
#define	 AHB_IMMED_OK		0xA0
#define	 AHB_ECB_ERR		0xC0
#define	 AHB_ASN		0xD0	/* for target mode */
#define	 AHB_IMMED_ERR		0xE0

#define	G2STAT_BUSY		0x01
#define	G2STAT_INT_PEND		0x02
#define	G2STAT_MBOX_EMPTY	0x04

#define	G2STAT2_HOST_READY	0x01

struct ahb_dma_seg {
	physaddr addr;
	long    len;
};

struct ahb_ecb_status {
	u_short status;
#define	ST_DON	0x0001
#define	ST_DU	0x0002
#define	ST_QF	0x0008
#define	ST_SC	0x0010
#define	ST_DO	0x0020
#define	ST_CH	0x0040
#define	ST_INT	0x0080
#define	ST_ASA	0x0100
#define	ST_SNS	0x0200
#define	ST_INI	0x0800
#define	ST_ME	0x1000
#define	ST_ECA	0x4000
	u_char  ha_status;
#define	HS_OK			0x00
#define	HS_CMD_ABORTED_HOST	0x04
#define	HS_CMD_ABORTED_ADAPTER	0x05
#define	HS_TIMED_OUT		0x11
#define	HS_HARDWARE_ERR		0x20
#define	HS_SCSI_RESET_ADAPTER	0x22
#define	HS_SCSI_RESET_INCOMING	0x23
	u_char  targ_status;
#define	TS_OK			0x00
#define	TS_CHECK_CONDITION	0x02
#define	TS_BUSY			0x08
	u_long  resid_count;
	u_long  resid_addr;
	u_short addit_status;
	u_char  sense_len;
	u_char  unused[9];
	u_char  cdb[6];
};


struct ecb {
	u_char  opcode;
#define	ECB_SCSI_OP	0x01
	        u_char:4;
	u_char  options:3;
	        u_char:1;
	short   opt1;
#define	ECB_CNE	0x0001
#define	ECB_DI	0x0080
#define	ECB_SES	0x0400
#define	ECB_S_G	0x1000
#define	ECB_DSB	0x4000
#define	ECB_ARS	0x8000
	short   opt2;
#define	ECB_LUN	0x0007
#define	ECB_TAG	0x0008
#define	ECB_TT	0x0030
#define	ECB_ND	0x0040
#define	ECB_DAT	0x0100
#define	ECB_DIR	0x0200
#define	ECB_ST	0x0400
#define	ECB_CHK	0x0800
#define	ECB_REC	0x4000
#define	ECB_NRB	0x8000
	u_short unused1;
	physaddr data;
	u_long  datalen;
	physaddr status;
	physaddr chain;
	short   unused2;
	short   unused3;
	physaddr sense;
	u_char  senselen;
	u_char  cdblen;
	short   cksum;
	u_char  cdb[12];
	/*-----------------end of hardware supported fields----------------*/
	struct ecb *next;	/* in free list */
	struct scsi_xfer *xs;	/* the scsi_xfer for this cmd */
	int     flags;
#define ECB_FREE	0
#define ECB_ACTIVE	1
#define ECB_ABORTED	2
#define ECB_IMMED	4
#define ECB_IMMED_FAIL	8
	struct ahb_dma_seg ahb_dma[AHB_NSEG];
	struct ahb_ecb_status ecb_status;
	struct scsi_sense_data ecb_sense;
	struct ecb *nexthash;
	physaddr hashkey;	/* physaddr of this struct */
};

struct ahb_data {
	int	unit;
	int     flags;
#define	AHB_INIT	0x01;
	int     baseport;
	struct ecb *ecbhash[ECB_HASH_SIZE];
	struct ecb *free_ecb;
	int     our_id;		/* our scsi id */
	int     vect;
	struct ecb *immed_ecb;	/* an outstanding immediete command */
	struct scsi_link sc_link;
	int     numecbs;
};

static u_int32_t ahb_adapter_info __P((int unit));
static struct ahb_data *
		ahb_alloc __P((int unit, u_long iobase, int irq));
static int	ahb_attach __P((struct eisa_device *dev));
static int	ahb_bus_attach __P((struct ahb_data *ahb));
static void	ahb_done __P((struct ahb_data *ahb, struct ecb *ecb, int state));
static void	ahb_free __P((struct ahb_data *ahb));
static void	ahb_free_ecb __P((struct ahb_data* ahb, struct ecb *ecb,
				  int flags));
static struct ecb *
		ahb_get_ecb __P((struct ahb_data* ahb, int flags));
static struct ecb *
		ahb_ecb_phys_kv __P((struct ahb_data *ahb, physaddr ecb_phys));
static int	ahb_init __P((struct ahb_data *ahb));
static void	ahbintr __P((void *arg));
static const char *ahbmatch __P((eisa_id_t type));
static void	ahbminphys __P((struct buf *bp));
static int	ahb_poll __P((struct ahb_data *ahb, int wait));
#ifdef AHBDEBUG
static void	ahb_print_active_ecb __P((struct ahb_data *ahb));
static void	ahb_print_ecb __P((struct ecb *ecb));
#endif
static int	ahbprobe __P((void));
static int	ahb_reset __P((u_long port));
static int32_t	ahb_scsi_cmd __P((struct scsi_xfer *xs));
static void	ahb_send_immed __P((struct ahb_data *ahb, int target,
				    u_long cmd));
static void	ahb_send_mbox __P((struct ahb_data *ahb, int opcode, int target,
				   struct ecb *ecb));
static timeout_t
		ahb_timeout;

static	struct	ecb *cheat;

static  u_long		ahb_unit = 0;
static 	int		ahb_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, ahb_debug, CTLFLAG_RW, &ahb_debug, 0, "");

#define AHB_SHOWECBS 0x01
#define AHB_SHOWINTS 0x02
#define AHB_SHOWCMDS 0x04
#define AHB_SHOWMISC 0x08
#define FAIL	1
#define SUCCESS 0
#define PAGESIZ 4096

#ifdef	KERNEL
static struct eisa_driver ahb_eisa_driver =
{
	"ahb",
	ahbprobe,
	ahb_attach,
	/*shutdown*/NULL,
	&ahb_unit
};

DATA_SET (eisadriver_set, ahb_eisa_driver);

static struct scsi_adapter ahb_switch =
{
	ahb_scsi_cmd,
	ahbminphys,
	0,
	0,
	ahb_adapter_info,
	"ahb",
	{ 0, 0 }
};

/* the below structure is so we have a default dev struct for our link struct */
static struct scsi_device ahb_dev =
{
    NULL,			/* Use default error handler */
    NULL,			/* have a queue, served by this */
    NULL,			/* have no async handler */
    NULL,			/* Use default 'done' routine */
    "ahb",
    0,
    { 0, 0 }
};

#endif /*KERNEL */

#ifndef	KERNEL
main()
{
	printf("ahb_data size is %d\n", sizeof(struct ahb_data));
	printf("ecb size is %d\n", sizeof(struct ecb));
}

#else /*KERNEL */

/*
 * Function to send a command out through a mailbox
 */
static void
ahb_send_mbox(struct ahb_data* ahb, int opcode, int target, struct ecb *ecb)
{
	int     port = ahb->baseport;
	int     wait = 300;	/* 3ms should be enough */
	int     stport = port + G2STAT;
	int     s = splbio();

	while (--wait) {
		if ((inb(stport) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
		    == (G2STAT_MBOX_EMPTY))
			break;
		DELAY(10);
	}
	if (wait == 0) {
		printf("ahb%d: board is not responding\n", ahb->unit);
		Debugger("aha1742");
		fatal_if_no_DDB();
	}
	outl(port + MBOXOUT0, KVTOPHYS(ecb));	/* don't know this will work */
	outb(port + ATTN, opcode | target);

	splx(s);
}

/*
 * Function to poll for command completion when in poll mode
 */
static int
ahb_poll(struct ahb_data *ahb, int wait)
{				/* in msec  */
	int     port = ahb->baseport;
	int     stport = port + G2STAT;

      retry:
	while (--wait) {
		if (inb(stport) & G2STAT_INT_PEND)
			break;
		DELAY(1000);
	} if (wait == 0) {
		printf("ahb%d: board is not responding\n", ahb->unit);
		return (EIO);
	}
	if (cheat != ahb_ecb_phys_kv(ahb, inl(port + MBOXIN0))) {
		printf("discarding 0x%lx ", inl(port + MBOXIN0));
		outb(port + G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
		DELAY(50000);
		goto retry;
	}
	/* don't know this will work */
	ahbintr((void *)ahb);
	return (0);
}

/*
 * Function to  send an immediate type command to the adapter
 */
static void
ahb_send_immed(struct ahb_data *ahb, int target, u_long cmd)
{
	int     port = ahb->baseport;
	int     s = splbio();
	int     stport = port + G2STAT;
	int     wait = 100;	/* 1 ms enough? */

	while (--wait) {
		if ((inb(stport) & (G2STAT_BUSY | G2STAT_MBOX_EMPTY))
		    == (G2STAT_MBOX_EMPTY))
			break;
		DELAY(10);
	} if (wait == 0) {
		printf("ahb%d: board is not responding\n", ahb->unit);
		Debugger("aha1742");
		fatal_if_no_DDB();
	}
	outl(port + MBOXOUT0, cmd);	/* don't know this will work */
	outb(port + G2CNTRL, G2CNTRL_SET_HOST_READY);
	outb(port + ATTN, OP_IMMED | target);
	splx(s);
}

static const char *
ahbmatch(type)     
	eisa_id_t type;
{                         
	switch(type & 0xfffffe00) {
		case EISA_DEVICE_ID_ADAPTEC_1740:
			return ("Adaptec 174x SCSI host adapter");
			break;
		default:
			break;
	}
	return (NULL);
} 

int
ahbprobe(void)      
{       
	u_long iobase;
	char intdef;      
	u_long irq;
	struct eisa_device *e_dev = NULL;
	int count;      
                
	count = 0;      
	while ((e_dev = eisa_match_dev(e_dev, ahbmatch))) {
		iobase = (e_dev->ioconf.slot * EISA_SLOT_SIZE) +
			 AHB_EISA_SLOT_OFFSET;
                        
		eisa_add_iospace(e_dev, iobase, AHB_EISA_IOSIZE, RESVADDR_NONE);
		intdef = inb(INTDEF + iobase);
		switch (intdef & 0x7) {
			case INT9:  
				irq = 9;
				break;
			case INT10: 
				irq = 10;
				break;
			case INT11:
				irq = 11;
				break;
			case INT12:
				irq = 12; 
				break;
			case INT14:
		                irq = 14;
				break;
			case INT15:
				irq = 15;
				break;
			default:
			        printf("aha174X at slot %d: illegal "
					"irq setting %d\n", e_dev->ioconf.slot,
					(intdef & 0x7));
                                continue;
		}               
		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &ahb_eisa_driver);
		count++;        
	}               
	return count;   
}

static struct ahb_data *
ahb_alloc(unit, iobase, irq)
	int	unit;
	u_long	iobase;
	int	irq;
{
	struct	ahb_data *ahb;

	/*
	 * Allocate a storage area for us
	 */
	ahb = malloc(sizeof(struct ahb_data), M_TEMP, M_NOWAIT);
	if (!ahb) {
		printf("ahb%d: cannot malloc!\n", unit);
		return NULL;
	}
	bzero(ahb, sizeof(struct ahb_data));
	ahb->unit = unit;
	ahb->baseport = iobase;
	ahb->vect = irq;

	return(ahb);
}

static void    
ahb_free(ahb)   
	struct ahb_data *ahb;  
{
	free(ahb, M_DEVBUF);
	return; 
}

/*
 * reset board, If it doesn't respond, return failure
 */
static int
ahb_reset(port)
	u_long port;
{
	u_char	i;
	int	wait = 1000;	/* 1 sec enough? */
	int	stport = port + G2STAT;

	outb(port + EBCTRL, CDEN);	/* enable full card */
	outb(port + PORTADDR, PORTADDR_ENHANCED);

	outb(port + G2CNTRL, G2CNTRL_HARD_RESET);
	DELAY(1000);
	outb(port + G2CNTRL, 0);
	DELAY(10000);
	while (--wait) {
		if ((inb(stport) & G2STAT_BUSY) == 0)
			break;
		DELAY(1000);
	} if (wait == 0) {
		printf("ahb_reset: No answer from aha1742 board\n");
		return (0);
	}
	i = inb(port + MBOXIN0) & 0xff;
	if (i) {
		printf("ahb_reset: self test failed, val = 0x%x\n", i);
		return (0);
	}
	while (inb(stport) & G2STAT_INT_PEND) {
		printf(".");
		outb(port + G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
		DELAY(10000);
	}
	outb(port + EBCTRL, CDEN);	/* enable full card */
	outb(port + PORTADDR, PORTADDR_ENHANCED);
	return (1);
}

/*
 * Attach ourselves and the devices on our bus
 */
static int
ahb_attach(e_dev)
	struct eisa_device *e_dev;
{
	/*
	 * find unit and check we have that many defined
	 */
	int	unit = e_dev->unit;
	struct	ahb_data *ahb;
	resvaddr_t *iospace;
	int	irq;

	if (TAILQ_FIRST(&e_dev->ioconf.irqs) == NULL)
		return (-1);

	irq = TAILQ_FIRST(&e_dev->ioconf.irqs)->irq_no;

	iospace = e_dev->ioconf.ioaddrs.lh_first;

	if(!iospace)
		return -1;

	if(!(ahb_reset(iospace->addr)))
		return -1;

	eisa_reg_start(e_dev);
	if(eisa_reg_iospace(e_dev, iospace)) {
		eisa_reg_end(e_dev);
		return -1;
	}

	if(!(ahb = ahb_alloc(unit, iospace->addr, irq))) {
		ahb_free(ahb);
		eisa_reg_end(e_dev);
		return -1;
	}

	if(eisa_reg_intr(e_dev, irq, ahbintr, (void *)ahb, &bio_imask,
			 /*shared ==*/TRUE)) {
		ahb_free(ahb);
		eisa_reg_end(e_dev);
		return -1;
	}
	eisa_reg_end(e_dev);

	/*
	 * Now that we know we own the resources we need, do the full
	 * card initialization.
	 */
	if(ahb_init(ahb)){
		ahb_free(ahb);
		/*
		 * The board's IRQ line will not be left enabled
		 * if we can't intialize correctly, so its safe
		 * to release the irq.
		 */
		eisa_release_intr(e_dev, irq, ahbintr);
		return -1;
        }

	/* Attach sub-devices - always succeeds */
	ahb_bus_attach(ahb);

	return(eisa_enable_intr(e_dev, irq));
}


/*
 * Attach all the sub-devices we can find
 */
static int
ahb_bus_attach(ahb)
	struct ahb_data *ahb;
{
	struct scsibus_data *scbus;

	/*
	 * fill in the prototype scsi_link.
	 */
	ahb->sc_link.adapter_unit = ahb->unit;
	ahb->sc_link.adapter_targ = ahb->our_id;
	ahb->sc_link.adapter_softc = ahb;
	ahb->sc_link.adapter = &ahb_switch;
	ahb->sc_link.device = &ahb_dev;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
		return 0;
	scbus->adapter_link = &ahb->sc_link;

	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);

	return 1;
}

/*
 * Return some information to the caller about
 * the adapter and it's capabilities
 */
static u_int32_t
ahb_adapter_info(unit)
	int     unit;
{
	return (2);		/* 2 outstanding requests at a time per device */
}

/*
 * Catch an interrupt from the adaptor
 */
static void
ahbintr(arg)
	void	*arg;
{
	struct ahb_data *ahb;
	struct ecb *ecb;
	unsigned char stat;
	u_char  ahbstat;
	int     target;
	long int mboxval;
	int	port;

	ahb = (struct ahb_data *)arg;
	port = ahb->baseport;

#ifdef	AHBDEBUG
	printf("ahbintr ");
#endif /*AHBDEBUG */

	while (inb(port + G2STAT) & G2STAT_INT_PEND) {
		/*
		 * First get all the information and then
		 * acknowlege the interrupt
		 */
		ahbstat = inb(port + G2INTST);
		target = ahbstat & G2INTST_TARGET;
		stat = ahbstat & G2INTST_INT_STAT;
		mboxval = inl(port + MBOXIN0);	/* don't know this will work */
		outb(port + G2CNTRL, G2CNTRL_CLEAR_EISA_INT);
#ifdef	AHBDEBUG
		printf("status = 0x%x ", stat);
#endif /*AHBDEBUG */
		/*
		 * Process the completed operation
		 */

		if (stat == AHB_ECB_OK) {	/* common case is fast */
			ecb = ahb_ecb_phys_kv(ahb, mboxval);
		} else {
			switch (stat) {
			case AHB_IMMED_OK:
				ecb = ahb->immed_ecb;
				ahb->immed_ecb = 0;
				break;
			case AHB_IMMED_ERR:
				ecb = ahb->immed_ecb;
				ecb->flags |= ECB_IMMED_FAIL;
				ahb->immed_ecb = 0;
				break;
			case AHB_ASN:	/* for target mode */
				printf("ahb%d: "
				    "Unexpected ASN interrupt(0x%lx)\n",
				    ahb->unit, mboxval);
				ecb = 0;
				break;
			case AHB_HW_ERR:
				printf("ahb%d: "
				    "Hardware error interrupt(0x%lx)\n",
				    ahb->unit, mboxval);
				ecb = 0;
				break;
			case AHB_ECB_RECOVERED:
				ecb = ahb_ecb_phys_kv(ahb, mboxval);
				break;
			case AHB_ECB_ERR:
				ecb = ahb_ecb_phys_kv(ahb, mboxval);
				break;
			default:
				printf(" Unknown return from ahb%d(%x)\n",
					ahb->unit, ahbstat);
				ecb = 0;
			}
		} if (ecb) {
#ifdef	AHBDEBUG
			if (ahb_debug & AHB_SHOWCMDS) {
				show_scsi_cmd(ecb->xs);
			}
			if ((ahb_debug & AHB_SHOWECBS) && ecb)
				printf("<int ecb(%x)>", ecb);
#endif /*AHBDEBUG */
			untimeout(ahb_timeout, (caddr_t)ecb);
			ahb_done(ahb, ecb, ((stat == AHB_ECB_OK) ? SUCCESS : FAIL));
		}
	}
}

/*
 * We have a ecb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
static void
ahb_done(ahb, ecb, state)
	struct ahb_data *ahb;
	int    state;
	struct ecb *ecb;
{
	struct ahb_ecb_status *stat = &ecb->ecb_status;
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs = ecb->xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahb_done\n"));
	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if (ecb->flags & ECB_IMMED) {
		if (ecb->flags & ECB_IMMED_FAIL) {
			xs->error = XS_DRIVER_STUFFUP;
		}
		goto done;
	}
	if ((state == SUCCESS) || (xs->flags & SCSI_ERR_OK)) {	/* All went correctly  OR errors expected */
		xs->resid = 0;
		xs->error = 0;
	} else {

		s1 = &(ecb->ecb_sense);
		s2 = &(xs->sense);

		if (stat->ha_status) {
			switch (stat->ha_status) {
			case HS_SCSI_RESET_ADAPTER:
				break;
			case HS_SCSI_RESET_INCOMING:
				break;
			case HS_CMD_ABORTED_HOST:	/* No response */
			case HS_CMD_ABORTED_ADAPTER:	/* No response */
				break;
			case HS_TIMED_OUT:	/* No response */
#ifdef	AHBDEBUG
				if (ahb_debug & AHB_SHOWMISC) {
					printf("timeout reported back\n");
				}
#endif /*AHBDEBUG */
				xs->error = XS_TIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				xs->error = XS_DRIVER_STUFFUP;
#ifdef	AHBDEBUG
				if (ahb_debug & AHB_SHOWMISC) {
					printf("unexpected ha_status: %x\n",
					    stat->ha_status);
				}
#endif /*AHBDEBUG */
			}
		} else {
			switch (stat->targ_status) {
			case TS_CHECK_CONDITION:
				/* structure copy!!!!! */
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case TS_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
#ifdef	AHBDEBUG
				if (ahb_debug & AHB_SHOWMISC) {
					printf("unexpected targ_status: %x\n",
					    stat->targ_status);
				}
#endif /*AHBDEBUG */
				xs->error = XS_DRIVER_STUFFUP;
			}
		}
	}
done:	xs->flags |= ITSDONE;
	ahb_free_ecb(ahb, ecb, xs->flags);
	scsi_done(xs);
}

/*
 * A ecb (and hence a mbx-out is put onto the
 * free list.
 */
static void
ahb_free_ecb(ahb, ecb, flags)
	struct	ahb_data *ahb;
	int	flags;
	struct	ecb *ecb;
{
	unsigned int opri = 0;

	if (!(flags & SCSI_NOMASK))
		opri = splbio();

	ecb->next = ahb->free_ecb;
	ahb->free_ecb = ecb;
	ecb->flags = ECB_FREE;
	/*
	 * If there were none, wake abybody waiting for
	 * one to come free, starting with queued entries
	 */
	if (!ecb->next) {
		wakeup((caddr_t)&ahb->free_ecb);
	}
	if (!(flags & SCSI_NOMASK))
		splx(opri);
}

/*
 * Get a free ecb
 * If there are none, see if we can allocate a
 * new one. If so, put it in the hash table too
 * otherwise either return an error or sleep
 */
static struct ecb *
ahb_get_ecb(ahb, flags)
	struct	ahb_data *ahb;
	int	flags;
{
	unsigned opri = 0;
	struct ecb *ecbp;
	int     hashnum;

	opri = splbio();
	/*
	 * If we can and have to, sleep waiting for one to come free
	 * but only if we can't allocate a new one.
	 */
	while (!(ecbp = ahb->free_ecb)) {
		if (ahb->numecbs < AHB_ECB_MAX) {
			ecbp = (struct ecb *) malloc(sizeof(struct ecb),
				M_TEMP,
				M_NOWAIT);
			if (ecbp) {
				bzero(ecbp, sizeof(struct ecb));
				ahb->numecbs++;
				ecbp->flags = ECB_ACTIVE;
				/*
				 * put in the phystokv hash table
				 * Never gets taken out.
				 */
				ecbp->hashkey = KVTOPHYS(ecbp);
				hashnum = ECB_HASH(ecbp->hashkey);
				ecbp->nexthash = ahb->ecbhash[hashnum];
				ahb->ecbhash[hashnum] = ecbp;
			} else {
				printf("ahb%d: Can't malloc ECB\n", ahb->unit);
			} 
			break;
		} else {
			if (!(flags & SCSI_NOSLEEP)) {
				tsleep((caddr_t)&ahb->free_ecb, PRIBIO,
				    "ahbecb", 0);
				continue;
			}
			break;
		}
	}
	if (ecbp) {
		/* Get ECB from from free list */
		ahb->free_ecb = ecbp->next;
		ecbp->flags = ECB_ACTIVE;
	}

	splx(opri);

	return (ecbp);
}

/*
 * given a physical address, find the ecb that
 * it corresponds to:
 */
static struct ecb *
ahb_ecb_phys_kv(ahb, ecb_phys)
	struct ahb_data *ahb;
	physaddr ecb_phys;
{
	int     hashnum = ECB_HASH(ecb_phys);
	struct ecb *ecbp = ahb->ecbhash[hashnum];

	while (ecbp) {
		if (ecbp->hashkey == ecb_phys)
			break;
		ecbp = ecbp->nexthash;
	}
	return ecbp;
}

/*
 * Start the board, ready for normal operation
 */
static int
ahb_init(ahb)
	struct	ahb_data *ahb;
{
	u_char intdef;
	int     port = ahb->baseport;

	/*
	 * Assume we have a board at this stage
	 * setup dma channel from jumpers and save int
	 * level
	 */
	intdef = inb(port + INTDEF);
	outb(port + INTDEF, (intdef | INTEN));	/* make sure we can interrupt */

	/* who are we on the scsi bus? */
	ahb->our_id = (inb(port + SCSIDEF) & HSCSIID);

	/*
	 * Note that we are going and return (to probe)
	 */
	ahb->flags |= AHB_INIT;
	return (0);
}

#ifndef	min
#define min(x,y) (x < y ? x : y)
#endif	/* min */

static void
ahbminphys(bp)
	struct buf *bp;
{
	if (bp->b_bcount > ((AHB_NSEG - 1) * PAGESIZ)) {
		bp->b_bcount = ((AHB_NSEG - 1) * PAGESIZ);
	}
}

/*
 * start a scsi operation given the command and
 * the data address. Also needs the unit, target
 * and lu
 */
static int32_t
ahb_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct ecb *ecb;
	struct ahb_dma_seg *sg;
	int     seg;		/* scatter gather seg being worked on */
	int     thiskv;
	physaddr thisphys, nextphys;
	int     bytes_this_seg, bytes_this_page, datalen, flags;
	struct ahb_data *ahb;
	int     s;

	ahb = (struct ahb_data *)xs->sc_link->adapter_softc;
	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahb_scsi_cmd\n"));
	/*
	 * get a ecb (mbox-out) to use. If the transfer
	 * is from a buf (possibly from interrupt time)
	 * then we can't allow it to sleep
	 */
	flags = xs->flags;
	if (xs->bp)
		flags |= (SCSI_NOSLEEP);	/* just to be sure */
	if (flags & ITSDONE) {
		printf("ahb%d: Already done?", ahb->unit);
		xs->flags &= ~ITSDONE;
	}
	if (!(flags & INUSE)) {
		printf("ahb%d: Not in use?", ahb->unit);
		xs->flags |= INUSE;
	}
	if (!(ecb = ahb_get_ecb(ahb, flags))) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	cheat = ecb;
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("start ecb(%p)\n", ecb));
	ecb->xs = xs;
	/*
	 * If it's a reset, we need to do an 'immediate'
	 * command, and store it's ecb for later
	 * if there is already an immediate waiting,
	 * then WE must wait
	 */
	if (flags & SCSI_RESET) {
		ecb->flags |= ECB_IMMED;
		if (ahb->immed_ecb) {
			return (TRY_AGAIN_LATER);
		}
		ahb->immed_ecb = ecb;
		if (!(flags & SCSI_NOMASK)) {
			s = splbio();
			ahb_send_immed(ahb, xs->sc_link->target, AHB_TARG_RESET);
			timeout(ahb_timeout, (caddr_t)ecb, (xs->timeout * hz) / 1000);
			splx(s);
			return (SUCCESSFULLY_QUEUED);
		} else {
			ahb_send_immed(ahb, xs->sc_link->target, AHB_TARG_RESET);
			/*
			 * If we can't use interrupts, poll on completion
			 */
			SC_DEBUG(xs->sc_link, SDEV_DB3, ("wait\n"));
			if (ahb_poll(ahb, xs->timeout)) {
				ahb_free_ecb(ahb, ecb, flags);
				xs->error = XS_TIMEOUT;
				return (HAD_ERROR);
			}
			return (COMPLETE);
		}
	}
	/*
	 * Put all the arguments for the xfer in the ecb
	 */
	ecb->opcode = ECB_SCSI_OP;
	ecb->opt1 = ECB_SES | ECB_DSB | ECB_ARS;
	if (xs->datalen) {
		ecb->opt1 |= ECB_S_G;
	}
	ecb->opt2 = xs->sc_link->lun | ECB_NRB;
	ecb->cdblen = xs->cmdlen;
	ecb->sense = KVTOPHYS(&(ecb->ecb_sense));
	ecb->senselen = sizeof(ecb->ecb_sense);
	ecb->status = KVTOPHYS(&(ecb->ecb_status));

	if (xs->datalen) {	/* should use S/G only if not zero length */
		ecb->data = KVTOPHYS(ecb->ahb_dma);
		sg = ecb->ahb_dma;
		seg = 0;
#ifdef	TFS
		if (flags & SCSI_DATA_UIO) {
			iovp = ((struct uio *) xs->data)->uio_iov;
			datalen = ((struct uio *) xs->data)->uio_iovcnt;
			xs->datalen = 0;
			while ((datalen) && (seg < AHB_NSEG)) {
				sg->addr = (physaddr) iovp->iov_base;
				xs->datalen += sg->len = iovp->iov_len;
				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("(0x%x@0x%x)", iovp->iov_len
					,iovp->iov_base));
				sg++;
				iovp++;
				seg++;
				datalen--;
			}
		}
		else
#endif /*TFS */
		{
			/*
			 * Set up the scatter gather block
			 */

			SC_DEBUG(xs->sc_link, SDEV_DB4,
			    ("%ld @%p:- ", xs->datalen, xs->data));
			datalen = xs->datalen;
			thiskv = (int) xs->data;
			thisphys = KVTOPHYS(thiskv);

			while ((datalen) && (seg < AHB_NSEG)) {
				bytes_this_seg = 0;

				/* put in the base address */
				sg->addr = thisphys;

				SC_DEBUGN(xs->sc_link, SDEV_DB4, ("0x%lx",
					thisphys));

				/* do it at least once */
				nextphys = thisphys;
				while ((datalen) && (thisphys == nextphys)) {
					/*
					 * This page is contiguous (physically) with
					 * the the last, just extend the length
					 */
					/* how far to the end of the page */
					nextphys = (thisphys & (~(PAGESIZ - 1)))
					    + PAGESIZ;
					bytes_this_page = nextphys - thisphys;
					/**** or the data ****/
					bytes_this_page = min(bytes_this_page
					    ,datalen);
					bytes_this_seg += bytes_this_page;
					datalen -= bytes_this_page;

					/* get more ready for the next page */
					thiskv = (thiskv & (~(PAGESIZ - 1)))
					    + PAGESIZ;
					if (datalen)
						thisphys = KVTOPHYS(thiskv);
				}
				/*
				 * next page isn't contiguous, finish the seg
				 */
				SC_DEBUGN(xs->sc_link, SDEV_DB4,
				    ("(0x%x)", bytes_this_seg));
				sg->len = bytes_this_seg;
				sg++;
				seg++;
			}
		} /*end of iov/kv decision */
		ecb->datalen = seg * sizeof(struct ahb_dma_seg);
		SC_DEBUGN(xs->sc_link, SDEV_DB4, ("\n"));
		if (datalen) {	/* there's still data, must have run out of segs! */
			printf("ahb_scsi_cmd%d: more than %d DMA segs\n",
			    ahb->unit, AHB_NSEG);
			xs->error = XS_DRIVER_STUFFUP;
			ahb_free_ecb(ahb, ecb, flags);
			return (HAD_ERROR);
		}
	} else {		/* No data xfer, use non S/G values */
		ecb->data = (physaddr) 0;
		ecb->datalen = 0;
	} ecb->chain = (physaddr) 0;
	/*
	 * Put the scsi command in the ecb and start it
	 */
	bcopy(xs->cmd, ecb->cdb, xs->cmdlen);
	/*
	 * Usually return SUCCESSFULLY QUEUED
	 */
	if (!(flags & SCSI_NOMASK)) {
		s = splbio();
		ahb_send_mbox(ahb, OP_START_ECB, xs->sc_link->target, ecb);
		timeout(ahb_timeout, (caddr_t)ecb, (xs->timeout * hz) / 1000);
		splx(s);
		SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_sent\n"));
		return (SUCCESSFULLY_QUEUED);
	}
	/*
	 * If we can't use interrupts, poll on completion
	 */
	ahb_send_mbox(ahb, OP_START_ECB, xs->sc_link->target, ecb);
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_wait\n"));
	do {
		if (ahb_poll(ahb, xs->timeout)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			ahb_send_mbox(ahb, OP_ABORT_ECB, xs->sc_link->target, ecb);
			if (ahb_poll(ahb, 2000)) {
				printf("abort failed in wait\n");
				ahb_free_ecb(ahb, ecb, flags);
			}
			xs->error = XS_DRIVER_STUFFUP;
			return (HAD_ERROR);
		}
	} while (!(xs->flags & ITSDONE));	/* something (?) else finished */
	if (xs->error) {
		return (HAD_ERROR);
	}
	return (COMPLETE);
}

static void
ahb_timeout(void *arg1)
{
	struct ecb * ecb = (struct ecb *)arg1;
	struct ahb_data *ahb;
	int     s = splbio();

	ahb = ecb->xs->sc_link->adapter_softc;
	printf("ahb%d:%d:%d (%s%d) timed out ", ahb->unit
	    ,ecb->xs->sc_link->target
	    ,ecb->xs->sc_link->lun
	    ,ecb->xs->sc_link->device->name
	    ,ecb->xs->sc_link->dev_unit);

#ifdef	AHBDEBUG
	if (ahb_debug & AHB_SHOWECBS)
		ahb_print_active_ecb(ahb);
#endif /*AHBDEBUG */

	/*
	 * If it's immediate, don't try abort it
	 */
	if (ecb->flags & ECB_IMMED) {
		ecb->xs->retries = 0;	/* I MEAN IT ! */
		ecb->flags |= ECB_IMMED_FAIL;
		ahb_done(ahb, ecb, FAIL);
		splx(s);
		return;
	}
	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ecb->flags == ECB_ABORTED) {
		/*
		 * abort timed out
		 */
		printf("AGAIN");
		ecb->xs->retries = 0;	/* I MEAN IT ! */
		ecb->ecb_status.ha_status = HS_CMD_ABORTED_HOST;
		ahb_done(ahb, ecb, FAIL);
	} else {		/* abort the operation that has timed out */
		printf("\n");
		ahb_send_mbox(ahb, OP_ABORT_ECB, ecb->xs->sc_link->target, ecb);
		/* 2 secs for the abort */
		timeout(ahb_timeout, (caddr_t)ecb, 2 * hz);
		ecb->flags = ECB_ABORTED;
	}
	splx(s);
}

#ifdef	AHBDEBUG
static void
ahb_print_ecb(ecb)
	struct ecb *ecb;
{
	printf("ecb:%x op:%x cmdlen:%d senlen:%d\n"
	    ,ecb
	    ,ecb->opcode
	    ,ecb->cdblen
	    ,ecb->senselen);
	printf("	datlen:%d hstat:%x tstat:%x flags:%x\n"
	    ,ecb->datalen
	    ,ecb->ecb_status.ha_status
	    ,ecb->ecb_status.targ_status
	    ,ecb->flags);
	show_scsi_cmd(ecb->xs);
}

static void
ahb_print_active_ecb(ahb)
	struct ahb_data *ahb;
{
	struct ecb *ecb;
	int     i = 0;

	while (i < ECB_HASH_SIZE) {
		ecb = ahb->ecbhash[i];
		while (ecb) {
			if (ecb->flags != ECB_FREE) {
				ahb_print_ecb(ecb);
			}
			ecb = ecb->nexthash;
		} i++;
	}
}
#endif /*AHBDEBUG */
#endif /*KERNEL */
