/*	$NetBSD: machdep.c,v 1.36.2.1 1999/04/16 16:24:16 chs Exp $ */

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)machdep.c	8.6 (Berkeley) 1/14/94
 */

#include "opt_bufcache.h"
#include "opt_compat_sunos.h"
#include "opt_compat_netbsd.h"
#include "opt_sysv.h"

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <uvm/uvm.h>	/* XXX: not _extern ... need vm_map_create */

#include <sys/sysctl.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/openfirm.h>
#include <machine/sparc64.h>
#include <machine/ctlreg.h>

#include <sparc64/sparc64/cache.h>
#include <sparc64/sparc64/vaddrs.h>

/* #include "fb.h" */

vm_map_t exec_map = NULL;
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;
extern vaddr_t avail_end;

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif

int	physmem;

extern	caddr_t msgbufaddr;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

/*
 * dvmamap is used to manage DVMA memory. Note: this coincides with
 * the memory range in `phys_map' (which is mostly a place-holder).
 */
vaddr_t dvma_base, dvma_end;
struct map *dvmamap;
static int ndvmamap;	/* # of entries in dvmamap */

caddr_t allocsys __P((caddr_t));
void	dumpsys __P((void));
void	stackdump __P((void));

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	unsigned i;
	caddr_t v;
	long sz;
	int base, residual;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;
#endif
	vaddr_t minaddr, maxaddr;
	vsize_t size;
	extern struct user *proc0paddr;

#ifdef DEBUG
	pmapdebug = 0;
#endif

	proc0.p_addr = proc0paddr;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	/*identifycpu();*/
	printf("real mem = %ld\n", (long)ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (long)allocsys((caddr_t)0);

	if ((v = (caddr_t)uvm_km_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

        /*
         * allocate virtual and physical memory for the buffers.
         */
        size = MAXBSIZE * nbuf;         /* # bytes for buffers */

        /* allocate VM for buffers... area is not managed by VM system */
        if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
                    NULL, UVM_UNKNOWN_OFFSET,
                    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
                                UVM_ADV_NORMAL, 0)) != KERN_SUCCESS)
        	panic("cpu_startup: cannot allocate VM for buffers");

        minaddr = (vaddr_t) buffers;
        if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
        	bufpages = btoc(MAXBSIZE) * nbuf; /* do not overallocate RAM */
        }
        base = bufpages / nbuf;
        residual = bufpages % nbuf;

        /* now allocate RAM for buffers */
	for (i = 0 ; i < nbuf ; i++) {
		vaddr_t curbuf;
		vsize_t curbufsize;
		struct vm_page *pg;

		/*
		 * each buffer has MAXBSIZE bytes of VM space allocated.  of
		 * that MAXBSIZE space we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t) buffers + (i * MAXBSIZE);
		curbufsize = CLBYTES * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: "
				    "not enough RAM for buffer cache");
			pmap_enter(kernel_map->pmap, curbuf,
			    VM_PAGE_TO_PHYS(pg), VM_PROT_READ|VM_PROT_WRITE,
			    TRUE, VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
        exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
                                 16*NCARGS, TRUE, FALSE, NULL);

	/*
	 * Allocate a map for physio.  Others use a submap of the kernel
	 * map, but we want one completely separate, even though it uses
	 * the same pmap.
	 */
	dvma_base = DVMA_BASE;
	dvma_end = DVMA_END;
	phys_map = uvm_map_create(pmap_kernel(), dvma_base, dvma_end, 1);
	if (phys_map == NULL)
		panic("unable to create DVMA map");
	/*
	 * Allocate DVMA space and dump into a privately managed
	 * resource map for double mappings which is usable from
	 * interrupt contexts.
	 */
	if (uvm_km_valloc_wait(phys_map, (dvma_end-dvma_base)) != dvma_base)
		panic("unable to allocate from DVMA map");
	rminit(dvmamap, btoc((dvma_end-dvma_base)),
		vtorc(dvma_base), "dvmamap", ndvmamap);

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
        mb_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
            VM_MBUF_SIZE, FALSE, FALSE, NULL);
	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %ld\n", ptoa(uvmexp.free));
	printf("using %ld buffers containing %ld bytes of memory\n",
		(long)nbuf, (long)bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

#if 0
	pmap_redzone();
#endif
}

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * You call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))
	valloc(callout, struct callout, ncallout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate (enough to
	 * hold 5% of total physical memory, but at least 16 and at
	 * most 1/2 of available kernel virtual memory).
	 * Allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0) {
		int bmax = btoc(VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
			   (MAXBSIZE/NBPG) / 2;
		bufpages = (physmem / 20) / CLSIZE;
		if (nbuf == 0 && bufpages > bmax)
			bufpages = bmax;
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(buf, struct buf, nbuf);
	/*
	 * Allocate DVMA slots for 1/4 of the number of i/o buffers
	 * and one for each process too (PHYSIO).
	 */
	valloc(dvmamap, struct map, ndvmamap = maxproc + ((nbuf / 4) &~ 1));
	return (v);
}

/*
 * Set up registers on exec.
 */

#ifdef __arch64__
#define rwindow		rwindow64
#define STACK_OFFSET	BIAS
#define CPOUTREG(l,v)	copyout(&(v), (l), sizeof(v))
#undef CCFSZ
#define CCFSZ	CC64FSZ
#else
#define rwindow		rwindow32
#define STACK_OFFSET	0
#define CPOUTREG(l,v)	copyout(&(v), (l), sizeof(v))
#endif

/* ARGSUSED */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	vaddr_t stack;
{
	register struct trapframe *tf = p->p_md.md_tf;
	register struct fpstate *fs;
	register int64_t tstate;

#if 0
	/* Make sure our D$ is not polluted w/bad data */
	blast_vcache();
#endif
	/* Don't allow misaligned code by default */
	p->p_md.md_flags &= ~MDP_FIXALIGN;

	/*
	 * Set the registers to 0 except for:
	 *	%o6: stack pointer, built in exec())
	 *	%tstate: (retain icc and xcc and cwp bits)
	 *	%g1: address of PS_STRINGS (used by crt0)
	 *	%tpc,%tnpc: entry point of program
	 */
	tstate = (ASI_PRIMARY_NO_FAULT<<TSTATE_ASI_SHIFT) |
		((PSTATE_USER)<<TSTATE_PSTATE_SHIFT) | 
		(tf->tf_tstate & TSTATE_CWP);
	if ((fs = p->p_md.md_fpstate) != NULL) {
		/*
		 * We hold an FPU state.  If we own *the* FPU chip state
		 * we must get rid of it, and the only way to do that is
		 * to save it.  In any case, get rid of our FPU state.
		 */
		if (p == fpproc) {
			savefpstate(fs);
			fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC);
		p->p_md.md_fpstate = NULL;
	}
	bzero((caddr_t)tf, sizeof *tf);
	tf->tf_tstate = tstate;
	tf->tf_global[1] = (vaddr_t)PS_STRINGS;
	/* %g4 needs to point to the start of the data segment */
	tf->tf_global[4] = 0; 
	tf->tf_pc = pack->ep_entry & ~3;
	tf->tf_npc = tf->tf_pc + 4;
	stack -= sizeof(struct rwindow);
	tf->tf_out[6] = stack - STACK_OFFSET;
	tf->tf_out[7] = NULL;
#ifdef NOTDEF_DEBUG
	printf("setregs: setting tf %p sp %p pc %p\n", (long)tf, 
	       (long)tf->tf_out[6], (long)tf->tf_pc);
	Debugger();
#endif
}

#ifdef DEBUG
/* See sigdebug.h */
#include <sparc64/sparc64/sigdebug.h>
int sigdebug = 0x0;
int sigpid = 0;
#endif

struct sigframe {
	int	sf_signo;		/* signal number */
	int	sf_code;		/* code */
#ifndef __arch64__
	struct	sigcontext *sf_scp;	/* SunOS user addr of sigcontext */
	int	sf_addr;		/* SunOS compat, always 0 for now */
#endif
	struct	sigcontext sf_sc;	/* actual sigcontext */
};

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig;
	sigset_t *mask;
	u_long code;
{
	struct proc *p = curproc;
	struct sigacts *psp = p->p_sigacts;
	struct sigframe *fp;
	struct trapframe *tf;
	vaddr_t addr; 
	struct rwindow *oldsp, *newsp;
#ifdef NOT_DEBUG
	struct rwindow tmpwin;
#endif
	struct sigframe sf;
	int onstack;

#if 0
	/* Make sure our D$ is not polluted w/bad data */
	blast_vcache();
#endif

	tf = p->p_md.md_tf;
	oldsp = (struct rwindow *)(tf->tf_out[6] + STACK_OFFSET);

	/* Do we need to jump onto the signal stack? */
	onstack =
	    (psp->ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (psp->ps_sigact[sig].sa_flags & SA_ONSTACK) != 0;

	/*
	 * Compute new user stack addresses, subtract off
	 * one signal frame, and align.
	 */
	if (onstack)
		fp = (struct sigframe *)((caddr_t)psp->ps_sigstk.ss_sp +
						  psp->ps_sigstk.ss_size);
	else
		fp = (struct sigframe *)oldsp;
	/* Allocate an aligned sigframe */
	fp = (struct sigframe *)((long)(fp - 1) & ~0x0f);

#ifdef DEBUG
	sigpid = p->p_pid;
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid) {
		printf("sendsig: %s[%d] sig %d newusp %p scp %p oldsp %p\n",
		    p->p_comm, p->p_pid, sig, fp, &fp->sf_sc, oldsp);
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif

	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	sf.sf_signo = sig;
	sf.sf_code = code;
#ifndef __arch64__
	sf.sf_scp = 0;
	sf.sf_addr = 0;			/* XXX */
#endif

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	sf.sf_sc.sc_mask = *mask;
#ifdef COMPAT_13
	/*
	 * XXX We always have to save an old style signal mask because
	 * XXX we might be delivering a signal to a process which will
	 * XXX escape from the signal in a non-standard way and invoke
	 * XXX sigreturn() directly.
	 */
	native_sigset_to_sigset13(mask, &sf.sf_sc.__sc_mask13);
#endif
	/* Save register context. */
	sf.sf_sc.sc_sp = (long)tf->tf_out[6];
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
	sf.sf_sc.sc_tstate = tf->tf_tstate; /* XXX */
	sf.sf_sc.sc_g1 = tf->tf_global[1];
	sf.sf_sc.sc_o0 = tf->tf_out[0];


	/*
	 * Put the stack in a consistent state before we whack away
	 * at it.  Note that write_user_windows may just dump the
	 * registers into the pcb; we need them in the process's memory.
	 * We also need to make sure that when we start the signal handler,
	 * its %i6 (%fp), which is loaded from the newly allocated stack area,
	 * joins seamlessly with the frame it was in when the signal occurred,
	 * so that the debugger and _longjmp code can back up through it.
	 */
	newsp = (struct rwindow *)((vaddr_t)fp - sizeof(struct rwindow));
	write_user_windows();
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK))
	    printf("sendsig: saving sf to %p, setting stack pointer %p to %p\n",
		   fp, &(((struct rwindow *)newsp)->rw_in[6]), (vaddr_t)tf->tf_out[6]);
#endif
	if (rwindow_save(p) || copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) || 
#ifdef NOT_DEBUG
	    copyin(oldsp, &tmpwin, sizeof(tmpwin)) || copyout(&tmpwin, newsp, sizeof(tmpwin)) ||
#endif
	    CPOUTREG(&(((struct rwindow *)newsp)->rw_in[6]), tf->tf_out[6])) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig: window save or copyout error\n");
		printf("sendsig: stack was trashed trying to send sig %d, sending SIGILL\n", sig);
		if (sigdebug & SDB_DDB) Debugger();
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
	}
#endif

	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
	addr = (vaddr_t)psp->ps_sigcode;
	tf->tf_global[1] = (vaddr_t)catcher;
	tf->tf_pc = addr;
	tf->tf_npc = addr + 4;
	tf->tf_out[6] = (vaddr_t)newsp - STACK_OFFSET;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid) {
		printf("sendsig: about to return to catcher %p thru %p\n", 
		       catcher, addr);
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above),
 * and return to the given trap frame (if there is one).
 * Check carefully to make sure that the user has not
 * modified the state to gain improper privileges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys___sigreturn14(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys___sigreturn14_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext sc, *scp;
	register struct trapframe *tf;
#ifndef TRAPWIN
	int i;
#endif

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
#if 0
	/* Make sure our D$ is not polluted w/bad data */
	blast_vcache();
#endif
	if (rwindow_save(p)) {
#ifdef DEBUG
		printf("sigreturn14: rwindow_save(%p) failed, sending SIGILL\n", p);
		Debugger();
#endif
		sigexit(p, SIGILL);
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sigreturn14: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif
	scp = SCARG(uap, sigcntxp);
 	if ((vaddr_t)scp & 3 || (copyin((caddr_t)scp, &sc, sizeof sc) != 0))
#ifdef DEBUG
	{
		printf("sigreturn14: copyin failed: scp=%p\n", scp);
		Debugger();
		return (EINVAL);
	}
#else
		return (EINVAL);
#endif
	scp = &sc;

	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((sc.sc_pc | sc.sc_npc) & 3) != 0 || (sc.sc_pc == 0) || (sc.sc_npc == 0))
#ifdef DEBUG
	{
		printf("sigreturn14: pc %p or npc %p invalid\n", sc.sc_pc, sc.sc_npc);
		Debugger();
		return (EINVAL);
	}
#else
		return (EINVAL);
#endif
	/* take only psr ICC field */
	tf->tf_tstate = (int64_t)(tf->tf_tstate & ~TSTATE_CCR) | (scp->sc_tstate & TSTATE_CCR);
	tf->tf_pc = (int64_t)scp->sc_pc;
	tf->tf_npc = (int64_t)scp->sc_npc;
	tf->tf_global[1] = (int64_t)scp->sc_g1;
	tf->tf_out[0] = (int64_t)scp->sc_o0;
	tf->tf_out[6] = (int64_t)scp->sc_sp;
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sigreturn14: return trapframe pc=%p sp=%p tstate=%llx\n",
		       (vaddr_t)tf->tf_pc, (vaddr_t)tf->tf_out[6], tf->tf_tstate);
		if (sigdebug & SDB_DDB) Debugger();
	}
#endif

	/* Restore signal stack. */
	if (sc.sc_onstack & SS_ONSTACK)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	(void) sigprocmask1(p, SIG_SETMASK, &sc.sc_mask, 0);

	return (EJUSTRETURN);
}

int	waittime = -1;

void
cpu_reboot(howto, user_boot_string)
	register int howto;
	char *user_boot_string;
{
	int i;
	static char str[128];
	extern int cold;

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

#if NFB > 0
	fb_unblank();
#endif
	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;

		/* XXX protect against curproc->p_stats.foo refs in sync() */
		if (curproc == NULL)
			curproc = &proc0;
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}
	(void) splhigh();		/* ??? */

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	/* If powerdown was requested, do it. */
	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
		/* Let the OBP do the work. */
		OF_poweroff();
		printf("WARNING: powerdown failed!\n");
		/*
		 * RB_POWERDOWN implies RB_HALT... fall into it...
		 */
	}

	if (howto & RB_HALT) {
		printf("halted\n\n");
		romhalt();
	}

	printf("rebooting\n\n");
	if (user_boot_string && *user_boot_string) {
		i = strlen(user_boot_string);
		if (i > sizeof(str))
			romboot(user_boot_string);	/* XXX */
		bcopy(user_boot_string, str, i);
	} else {
		i = 1;
		str[0] = '\0';
	}
			
	if (howto & RB_SINGLE)
		str[i++] = 's';
	if (howto & RB_KDB)
		str[i++] = 'd';
	if (i > 1) {
		if (str[0] == '\0')
			str[0] = '-';
		str[i] = 0;
	} else
		str[0] = 0;
	romboot(str);
	panic("cpu_reboot -- failed");
	/*NOTREACHED*/
}

u_long	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
cpu_dumpconf()
{
	register int nblks, dumpblks;

	if (dumpdev == NODEV || bdevsw[major(dumpdev)].d_psize == 0)
		/* No usable dump device */
		return;

	nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);

	dumpblks = ctod(physmem) + pmap_dumpsize();
	if (dumpblks > (nblks - ctod(1)))
		/*
		 * dump size is too big for the partition.
		 * Note, we safeguard a click at the front for a
		 * possible disk label.
		 */
		return;

	/* Put the dump at the end of the partition */
	dumplo = nblks - dumpblks;

	/*
	 * savecore(8) expects dumpsize to be the number of pages
	 * of actual core dumped (i.e. excluding the MMU stuff).
	 */
	dumpsize = physmem;
}

#define	BYTES_PER_DUMP	(8 * 1024)	/* must be a multiple of pagesize */
static vaddr_t dumpspace;

caddr_t
reserve_dumppages(p)
	caddr_t p;
{

	dumpspace = (vaddr_t)p;
	return (p + BYTES_PER_DUMP);
}

/*
 * Write a crash dump.
 */
void
dumpsys()
{
	register int psize;
	daddr_t blkno;
	register int (*dump)	__P((dev_t, daddr_t, caddr_t, size_t));
	int error = 0;
	register struct mem_region *mp;
	extern struct mem_region mem[];

	/* copy registers to memory */
	snapshot(cpcb);
	stackdump();

	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;

#if 0
	error = pmap_dumpmmu(dump, blkno);
	blkno += pmap_dumpsize();
#endif
	for (mp = mem; mp->size; mp++) {
		unsigned i = 0, n;
		paddr_t maddr = mp->start;

		if (maddr == 0) {
			/* Skip first page at physical address 0 */
			maddr += NBPG;
			i += NBPG;
			blkno += btodb(NBPG);
		}

		for (; i < mp->size; i += n) {
			n = mp->size - i;
			if (n > BYTES_PER_DUMP)
				 n = BYTES_PER_DUMP;

			/* print out how many MBs we have dumped */
			if (i && (i % (1024*1024)) == 0)
				printf("%d ", i / (1024*1024));

			(void) pmap_enter(pmap_kernel(), dumpspace, maddr,
					VM_PROT_READ, 1, VM_PROT_READ);
			error = (*dump)(dumpdev, blkno,
					(caddr_t)dumpspace, (int)n);
			pmap_remove(pmap_kernel(), dumpspace, dumpspace + n);
			if (error)
				break;
			maddr += n;
			blkno += btodb(n);
		}
	}

	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
}

void trapdump __P((struct trapframe*));
/*
 * dump out a trapframe.
 */
void
trapdump(tf)
	struct trapframe* tf;
{
	printf("TRAPFRAME: tstate=%x:%x pc=%x:%x npc=%x:%x y=%x\n",
	       tf->tf_tstate, tf->tf_pc, tf->tf_npc, tf->tf_y);
	printf("%%g1-7: %x:%x %x:%x %x:%x %x:%x %x:%x %x:%x %x:%x\n",
	       tf->tf_global[1], tf->tf_global[2], tf->tf_global[3], 
	       tf->tf_global[4], tf->tf_global[5], tf->tf_global[6], 
	       tf->tf_global[7]);
	printf("%%o0-7: %x:%x %x:%x %x:%x %x:%x\n %x:%x %x:%x %x:%x %x:%x\n",
	       tf->tf_out[0], tf->tf_out[1], tf->tf_out[2], tf->tf_out[3], 
	       tf->tf_out[4], tf->tf_out[5], tf->tf_out[6], tf->tf_out[7]);
}
/*
 * get the fp and dump the stack as best we can.  don't leave the
 * current stack page
 */
void
stackdump()
{
	struct frame32 *fp = (struct frame32 *)getfp(), *sfp;
	struct frame64 *fp64;

	sfp = fp;
	printf("Frame pointer is at %p\n", fp);
	printf("Call traceback:\n");
	while (fp && ((u_long)fp >> PGSHIFT) == ((u_long)sfp >> PGSHIFT)) {
		if( ((long)fp) & 1 ) {
			fp64 = (struct frame64*)(((char*)fp)+BIAS);
			/* 64-bit frame */
			printf("%x(%llx,%llx,%llx,%llx,%llx,%llx,%llx)sp=%p",
			       fp64->fr_pc, fp64->fr_arg[0], fp64->fr_arg[1], fp64->fr_arg[2],
			       fp64->fr_arg[3], fp64->fr_arg[4], fp64->fr_arg[5], fp64->fr_arg[6],
			       fp64->fr_fp);
			fp = (struct frame32*)fp64->fr_fp;
		} else {
			/* 32-bit frame */
			printf("  pc = %x  args = (%x, %x, %x, %x, %x, %x, %x) fp = %p\n",
			       fp->fr_pc, fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
			       fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6],
			       fp->fr_fp);
			fp = (struct frame32*)(u_long)(u_short)fp->fr_fp;
		}
	}
}


int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	return (ENOEXEC);
}

void
wzero(vb, l)
	void *vb;
	u_int l;
{
	u_char *b = vb;
	u_char *be = b + l;
	u_short *sp;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b & 1)
		*b++ = 0;

	/* back, */
	if (b != be && ((u_long)be & 1) != 0) {
		be--;
		*be = 0;
	}

	/* and middle. */
	sp = (u_short *)b;
	while (sp != (u_short *)be)
		*sp++ = 0;
}

void
wcopy(vb1, vb2, l)
	const void *vb1;
	void *vb2;
	u_int l;
{
	const u_char *b1e, *b1 = vb1;
	u_char *b2 = vb2;
	u_short *sp;
	int bstore = 0;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b1 & 1) {
		*b2++ = *b1++;
		l--;
	}

	/* middle, */
	sp = (u_short *)b1;
	b1e = b1 + l;
	if (l & 1)
		b1e--;
	bstore = (u_long)b2 & 1;

	while (sp < (u_short *)b1e) {
		if (bstore) {
			b2[1] = *sp & 0xff;
			b2[0] = *sp >> 8;
		} else
			*((short *)b2) = *sp;
		sp++;
		b2 += 2;
	}

	/* and back. */
	if (l & 1)
		*b2 = *b1e;
}

bus_addr_t dvmamap_alloc __P((int, int));

bus_addr_t
dvmamap_alloc(size, flags)
	int size;
	int flags;
{
	int s, pn, npf;

	npf = btoc(size);
	s = splimp();
	for (;;) {
		pn = rmalloc(dvmamap, npf);
		if (pn != 0)
			break;

		if (flags & BUS_DMA_WAITOK) {
			(void)tsleep(dvmamap, PRIBIO+1, "dvma", 0);
			continue;
		}
		splx(s);
		return ((bus_addr_t)-1);
	}
	splx(s);

	return ((bus_addr_t)rctov(pn));
}

void dvmamap_free __P((bus_addr_t, bus_size_t));

void
dvmamap_free (addr, size)
	bus_addr_t addr;
	bus_size_t size;
{
	int s, pn, npf;
	vaddr_t a = addr;
	u_int sz = size;

	npf = btoc(sz);
	pn = vtorc(a);
#ifdef DEBUG
	if (npf <= 0 || pn <= 0) {
		printf("dvmamap_free: npf=%d pn=%p a=%p sz=%d\n");
		Debugger();
		return;
	}
#endif
	s = splimp();
	rmfree(dvmamap, npf, pn);
	wakeup(dvmamap);
	splx(s);
}

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_bus_dmamap_create(t, size, nsegments, maxsegsz, boundary, flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct sparc_bus_dmamap *map;
	void *mapstore;
	size_t mapsize;

#ifdef DIAGNOSTIC
	if (nsegments != 1) 
		printf("_bus_dmamap_create(): sparc only supports one segment!\n");
#endif
	/*
	 * Allocate and initialize the DMA map.  The end of the map
	 * is a variable-sized array of segments, so we allocate enough
	 * room for them in one shot.
	 *
	 * Note we don't preserve the WAITOK or NOWAIT flags.  Preservation
	 * of ALLOCNOW notifies others that we've reserved these resources,
	 * and they are not to be freed.
	 *
	 * The bus_dmamap_t includes one bus_dma_segment_t, hence
	 * the (nsegments - 1).
	 */
	mapsize = sizeof(struct sparc_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));
	if ((mapstore = malloc(mapsize, M_DMAMAP,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	bzero(mapstore, mapsize);
	map = (struct sparc_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT|BUS_DMA_COHERENT|BUS_DMA_NOWRITE|BUS_DMA_NOCACHE);
	map->dm_mapsize = 0;		/* no valid mappings */
	map->dm_nsegs = 0;

	*dmamp = map;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
void
_bus_dmamap_destroy(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{

	free(map, M_DMAMAP);
}

/*
 * Common function for loading a DMA map with a linear buffer.  May
 * be called by bus-specific DMA map load functions.
 */
int
_bus_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	bus_size_t sgsize;
	bus_addr_t dvmaddr;
	vaddr_t vaddr = (vaddr_t)buf;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	if (buflen > map->_dm_size)
#ifdef DEBUG
	{ 
		printf("_bus_dmamap_load(): error %d > %d -- map size exceeded!\n", buflen, map->_dm_size);
		Debugger();
		return (EINVAL);
	}		
#else	
		return (EINVAL);
#endif

	sgsize = round_page(buflen + ((int)vaddr & PGOFSET));

	/*
	 * XXX Need to implement "don't dma across this boundry".
	 */
	dvmaddr = dvmamap_alloc(sgsize, flags);
#ifdef DEBUG
	if (dvmaddr == (bus_addr_t)-1)	
	{ 
		printf("_bus_dmamap_load(): dvmamap_alloc(%d, %x) failed!\n", sgsize, flags);
		Debugger();
	}		
#endif	
	if (dvmaddr == (bus_addr_t)-1)
		return (ENOMEM);

	/*
	 * We always use just one segment.
	 */
	map->dm_mapsize = buflen;
	map->dm_nsegs = 1;
	map->dm_segs[0].ds_addr = dvmaddr + (vaddr & PGOFSET);
	map->dm_segs[0].ds_len = sgsize;

	/* Mapping is bus dependent */
	return (0);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
_bus_dmamap_load_mbuf(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{

	panic("_bus_dmamap_load: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
_bus_dmamap_load_uio(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{

	panic("_bus_dmamap_load_uio: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_bus_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{

	panic("_bus_dmamap_load_raw: not implemented");
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
void
_bus_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	bus_addr_t dvmaddr;
	bus_size_t sgsize;

	if (map->dm_nsegs != 1)
		panic("_bus_dmamap_unload: nsegs = %d", map->dm_nsegs);

	dvmaddr = (map->dm_segs[0].ds_addr & ~PGOFSET);
	sgsize = map->dm_segs[0].ds_len;

	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	
	/* Unmapping is bus dependent */
	dvmamap_free(dvmaddr, sgsize);

	cache_flush((caddr_t)dvmaddr, (u_int) sgsize);	
}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
void
_bus_dmamap_sync(t, map, offset, len, ops)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{

	/*
	 * We sync out our caches, but the bus must do the same.
	 *
	 * Actually a #Sync is expensive.  We should optimize.
	 */
	switch (ops) {
	case BUS_DMASYNC_PREREAD:
		/* Flush any pending writes */
		__asm("membar #Sync" : );
		break;
	case BUS_DMASYNC_POSTREAD:
		/* Invalidate the vcache */
		blast_vcache();
		/* Maybe we should flush the I$? */
		break;
	case BUS_DMASYNC_PREWRITE:
		/* Flush any pending writes */
		__asm("membar #Sync" : );
		break;
	case BUS_DMASYNC_POSTWRITE:
		/* Nothing to do */
		break;
	default:
		printf("_bus_dmamap_sync: unknown sync op\n");
	}
}

extern paddr_t   vm_first_phys, vm_num_phys;
/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
int
_bus_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	vaddr_t low, high;
	struct pglist *mlist;
	int error;

	/* Always round the size. */
	size = round_page(size);
	low = vm_first_phys;
	high = vm_first_phys + vm_num_phys - PAGE_SIZE;

	if ((mlist = malloc(sizeof(*mlist), M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	/*
	 * Allocate pages from the VM system.
	 */
	TAILQ_INIT(mlist);
	error = uvm_pglistalloc(size, low, high,
	    alignment, boundary, mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
	if (error)
		return (error);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	segs[0].ds_addr= NULL; /* UPA does not map things */
	segs[0].ds_len = size;
	*rsegs = 1;

	/*
	 * Simply keep a pointer around to the linked list, so
	 * bus_dmamap_free() can return it.
	 *
	 * NOBODY SHOULD TOUCH THE pageq FIELDS WHILE THESE PAGES
	 * ARE IN OUR CUSTODY.
	 */
	segs[0]._ds_mlist = mlist;

	/* The bus driver should do the actual mapping */
	return (0);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
_bus_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{

	if (nsegs != 1)
		panic("bus_dmamem_free: nsegs = %d", nsegs);

	/*
	 * Return the list of pages back to the VM system.
	 */
	uvm_pglistfree(segs[0]._ds_mlist);
	free(segs[0]._ds_mlist, M_DEVBUF);
}

/*
 * Common function for mapping DMA-safe memory.  May be called by
 * bus-specific DMA memory map functions.
 */
int
_bus_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vaddr_t va, sva;
	struct pglist *mlist;
	int r, cbit;
	size_t oversize;
	u_long align;
#if 0
	/* This went away with dvma_mapin.  We may need it later */
	extern u_long dvma_cachealign;
#endif

	if (nsegs != 1)
		panic("_bus_dmamem_map: nsegs = %d", nsegs);

	cbit = PMAP_NC;
#if 0
	/* This went away with dvma_mapin.  We may need it later */
	align = dvma_cachealign ? dvma_cachealign : PAGE_SIZE;
#else
	align = PAGE_SIZE;
#endif

	size = round_page(size);

	/*
	 * Find a region of kernel virtual addresses that can accomodate
	 * our aligment requirements.
	 */
	oversize = size + align - PAGE_SIZE;
	r = uvm_map(kernel_map, &sva, oversize, NULL, UVM_UNKNOWN_OFFSET,
	    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	    UVM_ADV_NORMAL, 0));
	if (r != KERN_SUCCESS)
		return (ENOMEM);

	/* Compute start of aligned region */
	va = sva;
	va += ((segs[0].ds_addr & (align - 1)) + align - va) & (align - 1);

	/* Return excess virtual addresses */
	if (va != sva)
		(void)uvm_unmap(kernel_map, sva, va);
	if (va + size != sva + oversize)
		(void)uvm_unmap(kernel_map, va + size, sva + oversize);


	*kvap = (caddr_t)va;
	mlist = segs[0]._ds_mlist;

#if 0 /* The following should be done by the bus driver */
	for (m = mlist->tqh_first; m != NULL; m = m->pageq.tqe_next) {

		if (size == 0)
			panic("_bus_dmamem_map: size botch");

		addr = VM_PAGE_TO_PHYS(m);
		pmap_enter(pmap_kernel(), va, addr | cbit,
			   VM_PROT_READ | VM_PROT_WRITE, TRUE, 0);
#if 0
			if (flags & BUS_DMA_COHERENT)
				/* XXX */;
#endif
		va += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
#endif

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_bus_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{

#ifdef DIAGNOSTIC
	if ((u_long)kva & PGOFSET)
		panic("_bus_dmamem_unmap");
#endif

	size = round_page(size);
	uvm_unmap(kernel_map, (vaddr_t)kva, (vaddr_t)kva + size);
#if 0
	kmem_free(kernel_map, (vaddr_t)kva, size);
#endif
}

/*
 * Common functin for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
int
_bus_dmamem_mmap(t, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs, off, prot, flags;
{

	panic("_bus_dmamem_mmap: not implemented");
}


struct sparc_bus_dma_tag mainbus_dma_tag = {
	NULL,
	NULL,
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,

	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap
};


/*
 * Base bus space handlers.
 */
static int	sparc_bus_map __P(( bus_space_tag_t, bus_type_t, bus_addr_t,
				    bus_size_t, int, vaddr_t,
				    bus_space_handle_t *));
static int	sparc_bus_unmap __P((bus_space_tag_t, bus_space_handle_t,
				     bus_size_t));
static int	sparc_bus_mmap __P((bus_space_tag_t, bus_type_t,
				    bus_addr_t, int, bus_space_handle_t *));
static void	*sparc_mainbus_intr_establish __P((bus_space_tag_t, int, int,
						   int (*) __P((void *)),
						   void *));
static void     sparc_bus_barrier __P(( bus_space_tag_t, bus_space_handle_t,
					bus_size_t, bus_size_t, int));


int
sparc_bus_map(t, iospace, addr, size, flags, vaddr, hp)
	bus_space_tag_t t;
	bus_type_t	iospace;
	bus_addr_t	addr;
	bus_size_t	size;
	vaddr_t	vaddr;
	bus_space_handle_t *hp;
{
	vaddr_t v;
	u_int64_t pa;
	paddr_t	pm_flags;
static	vaddr_t iobase = IODEV_BASE;


	if (iobase == NULL)
		iobase = IODEV_BASE;

	size = round_page(size);
	if (size == 0) {
		printf("sparc_bus_map: zero size\n");
		return (EINVAL);
	}

	if (vaddr)
		v = trunc_page(vaddr);
	else {
		v = iobase;
		iobase += size;
		if (iobase > IODEV_END)	/* unlikely */
			panic("sparc_bus_map: iobase=0x%lx", iobase);
	}

	/* note: preserve page offset */
	*hp = (bus_space_handle_t)(v | ((u_long)addr & PGOFSET));

	pa = addr & ~PAGE_MASK; /* = trunc_page(addr); Will drop high bits */

#ifdef NOTDEF_DEBUG
	printf("\nsparc_bus_map: type %x addr %p virt %p paddr %llx\n",
		       iospace, addr, *hp, (paddr_t)pa);
#endif
	switch (iospace) {
	case PCI_CONFIG_BUS_SPACE:
		pm_flags = PMAP_NC|PMAP_LITTLE;
		break;
	case PCI_MEMORY_BUS_SPACE:
		pm_flags = PMAP_LITTLE;
		break;
	default:
		pm_flags = PMAP_NC;
		break;
	}

	do {
#ifdef NOTDEF_DEBUG
		printf("sparc_bus_map: phys %llx virt %p hp %llx\n", 
		       (int)(pa>>32), (int)pa, v, (int)((*hp)>>32), (int)*hp);
#endif
		pmap_enter(pmap_kernel(), v, pa | pm_flags,
				(flags&BUS_SPACE_MAP_READONLY) ? VM_PROT_READ
				: VM_PROT_READ | VM_PROT_WRITE, 1, 0);
		v += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((size -= PAGE_SIZE) > 0);
	return (0);
}

int
sparc_bus_unmap(t, bh, size)
	bus_space_tag_t t;
	bus_size_t	size;
	bus_space_handle_t bh;
{
	vaddr_t va = trunc_page((vaddr_t)bh);
	vaddr_t endva = va + round_page(size);

	pmap_remove(pmap_kernel(), va, endva);
	return (0);
}

int
sparc_bus_mmap(t, iospace, paddr, flags, hp)
	bus_space_tag_t t;
	bus_type_t	iospace;
	bus_addr_t	paddr;
	int		flags;
	bus_space_handle_t *hp;
{
#if 0
	*hp = (bus_space_handle_t)pmap_from_phys_address(paddr,flags);
#else
	*hp = (bus_space_handle_t)(paddr>>PGSHIFT);
#endif
#if 0
	printf("sparc_bus_mmap: encoding pa %llx as %llx becomes %llx\n",
	       (bus_addr_t)(paddr), (bus_space_handle_t)*hp, 
	       (paddr_t)(pmap_phys_address(*hp)));
#endif
	return (0);
}

/*
 * Establish a temporary bus mapping for device probing.  */
int
bus_space_probe(tag, btype, paddr, size, offset, flags, callback, arg)
	bus_space_tag_t tag;
	bus_type_t	btype;
	bus_addr_t	paddr;
	bus_size_t	size;
	size_t		offset;
	int		flags;
	int		(*callback) __P((void *, void *));
	void		*arg;
{
	bus_space_handle_t bh;
	caddr_t tmp;
	int result;

	if (bus_space_map2(tag, btype, paddr, size, flags, TMPMAP_VA, &bh) != 0)
		return (0);

	tmp = (caddr_t)bh;
	result = (probeget(tmp + offset, size) != -1);
	if (result && callback != NULL)
		result = (*callback)(tmp, arg);
	bus_space_unmap(tag, bh, size);
	return (result);
}


void *
sparc_mainbus_intr_establish(t, level, flags, handler, arg)
	bus_space_tag_t t;
	int	level;
	int	flags;
	int	(*handler)__P((void *));
	void	*arg;
{
	struct intrhand *ih;

	ih = (struct intrhand *)
		malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	if ((flags & BUS_INTR_ESTABLISH_FASTTRAP) != 0)
		intr_fasttrap(level, (void (*)__P((void)))handler);
	else
		intr_establish(level, ih);
	return (ih);
}

void sparc_bus_barrier (t, h, offset, size, flags)
	bus_space_tag_t	t;
	bus_space_handle_t h;
	bus_size_t	offset;
	bus_size_t	size;
	int		flags;
{
	/* 
	 * We have lots of alternatives depending on whether we're
	 * synchronizing loads with loads, loads with stores, stores
	 * with loads, or stores with stores.  The only ones that seem
	 * generic are #Sync and #MemIssue.  I'll use #Sync for safety.
	 */
	if (flags == (BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE))
		__asm("membar #Sync" : );
	else if (flags == BUS_SPACE_BARRIER_READ)
		__asm("membar #Sync" : );
	else if (flags == BUS_SPACE_BARRIER_WRITE)
		__asm("membar #Sync" : );
	else
		printf("sparc_bus_barrier: unknown flags\n");
	return;
}

struct sparc_bus_space_tag mainbus_space_tag = {
	NULL,				/* cookie */
	NULL,				/* parent bus tag */
	UPA_BUS_SPACE,			/* type (ASI) */
	sparc_bus_map,			/* bus_space_map */
	sparc_bus_unmap,		/* bus_space_unmap */
	NULL,				/* bus_space_subregion */
	sparc_bus_barrier,		/* bus_space_barrier */
	sparc_bus_mmap,			/* bus_space_mmap */
	sparc_mainbus_intr_establish	/* bus_intr_establish */
};
