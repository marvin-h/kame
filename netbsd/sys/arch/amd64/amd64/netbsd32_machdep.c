/*	$NetBSD: netbsd32_machdep.c,v 1.10 2004/03/25 15:32:27 drochner Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: netbsd32_machdep.c,v 1.10 2004/03/25 15:32:27 drochner Exp $");

#include "opt_compat_netbsd.h"
#include "opt_execfmt.h"
#include "opt_user_ldt.h"

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/ras.h>

#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#include <machine/mtrr.h>
#include <machine/netbsd32_machdep.h>
#include <machine/sysarch.h>

#include <compat/netbsd32/netbsd32.h>
#include <compat/netbsd32/netbsd32_exec.h>
#include <compat/netbsd32/netbsd32_syscallargs.h>

/* Provide a the name of the architecture we're emulating */
char	machine_arch32[] = "i386";	

int process_read_fpregs32(struct lwp *, struct fpreg32 *);
int process_read_regs32(struct lwp *, struct reg32 *);

extern void (osyscall_return) __P((void));

static int x86_64_get_mtrr32(struct lwp *, void *, register_t *);
static int x86_64_set_mtrr32(struct lwp *, void *, register_t *);

static int check_sigcontext32(const struct netbsd32_sigcontext *,
    struct trapframe *);
static int check_mcontext32(const mcontext32_t *, struct trapframe *);

#ifdef EXEC_AOUT
/*
 * There is no native a.out -- this function is required
 * for i386 a.out emulation (COMPAT_NETBSD32+EXEC_AOUT).
 */
int
cpu_exec_aout_makecmds(struct proc *p, struct exec_package *e)
{

	return ENOEXEC;
}
#endif

#ifdef COMPAT_16
/*
 * There is no NetBSD-1.6 compatibility for native code.
 * COMPAT_16 is useful for i386 emulation (COMPAT_NETBSD32) only.
 */
int
compat_16_sys___sigreturn14(struct lwp *l, void *v, register_t *retval)
{

	return ENOSYS;
}
#endif

void
netbsd32_setregs(struct lwp *l, struct exec_package *pack, u_long stack)
{
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct trapframe *tf;
	struct proc *p = l->l_proc;
	void **retaddr;

	/* If we were using the FPU, forget about it. */
	if (l->l_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_lwp(l, 0);

#if defined(USER_LDT) && 0
	pmap_ldt_cleanup(p);
#endif

	l->l_md.md_flags &= ~MDP_USEDFPU;
	pcb->pcb_flags = 0;
        pcb->pcb_savefpu.fp_fxsave.fx_fcw = __NetBSD_NPXCW__;
        pcb->pcb_savefpu.fp_fxsave.fx_mxcsr = __INITIAL_MXCSR__;  
	pcb->pcb_savefpu.fp_fxsave.fx_mxcsr_mask = __INITIAL_MXCSR_MASK__;


	l->l_proc->p_flag |= P_32;

	tf = l->l_md.md_regs;
	tf->tf_ds = LSEL(LUDATA32_SEL, SEL_UPL);
	tf->tf_es = LSEL(LUDATA32_SEL, SEL_UPL);
	tf->tf_fs = LSEL(LUDATA32_SEL, SEL_UPL);
	tf->tf_gs = LSEL(LUDATA32_SEL, SEL_UPL);
	tf->tf_rdi = 0;
	tf->tf_rsi = 0;
	tf->tf_rbp = 0;
	tf->tf_rbx = (u_int64_t)p->p_psstr;
	tf->tf_rdx = 0;
	tf->tf_rcx = 0;
	tf->tf_rax = 0;
	tf->tf_rip = pack->ep_entry;
	tf->tf_cs = LSEL(LUCODE32_SEL, SEL_UPL);
	tf->tf_rflags = PSL_USERSET;
	tf->tf_rsp = stack;
	tf->tf_ss = LSEL(LUDATA32_SEL, SEL_UPL);

	/* XXX frob return address to return via old iret method, not sysret */
	retaddr = (void **)tf - 1;
	*retaddr = (void *)osyscall_return;
}

#ifdef COMPAT_16
static void
netbsd32_sendsig_sigcontext(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct trapframe *tf;
	int sig = ksi->ksi_signo;
	sig_t catcher = SIGACTION(p, sig).sa_handler;
	struct netbsd32_sigframe_sigcontext *fp, frame;
	int onstack;
	struct sigacts *ps = p->p_sigacts;

	tf = l->l_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/* Allocate space for the signal handler context. */
	if (onstack)
		fp = (struct netbsd32_sigframe_sigcontext *)
		    ((caddr_t)p->p_sigctx.ps_sigstk.ss_sp +
					  p->p_sigctx.ps_sigstk.ss_size);
	else
		fp = (struct netbsd32_sigframe_sigcontext *)tf->tf_rsp;
	fp--;

	/* Build stack frame for signal trampoline. */
	switch (ps->sa_sigdesc[sig].sd_vers) {
	case 0:
		frame.sf_ra = (uint32_t)(u_long)p->p_sigctx.ps_sigcode;
		break;
	case 1:
		frame.sf_ra = (uint32_t)(u_long)ps->sa_sigdesc[sig].sd_tramp;
		break;
	default:
		/* Don't know what trampoline version; kill it. */
		sigexit(l, SIGILL);
	}
	frame.sf_signum = sig;
	frame.sf_code = ksi->ksi_trap;
	frame.sf_scp = (u_int32_t)(u_long)&fp->sf_sc;

	frame.sf_sc.sc_ds = tf->tf_ds;
	frame.sf_sc.sc_es = tf->tf_es;
	frame.sf_sc.sc_fs = tf->tf_fs;
	frame.sf_sc.sc_gs = tf->tf_gs;

	frame.sf_sc.sc_eflags = tf->tf_rflags;
	frame.sf_sc.sc_edi = tf->tf_rdi;
	frame.sf_sc.sc_esi = tf->tf_rsi;
	frame.sf_sc.sc_ebp = tf->tf_rbp;
	frame.sf_sc.sc_ebx = tf->tf_rbx;
	frame.sf_sc.sc_edx = tf->tf_rdx;
	frame.sf_sc.sc_ecx = tf->tf_rcx;
	frame.sf_sc.sc_eax = tf->tf_rax;
	frame.sf_sc.sc_eip = tf->tf_rip;
	frame.sf_sc.sc_cs = tf->tf_cs;
	frame.sf_sc.sc_esp = tf->tf_rsp;
	frame.sf_sc.sc_ss = tf->tf_ss;
	frame.sf_sc.sc_trapno = tf->tf_trapno;
	frame.sf_sc.sc_err = tf->tf_err;

	/* Save signal stack. */
	frame.sf_sc.sc_onstack = p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK;

	/* Save signal mask. */
	frame.sf_sc.sc_mask = *mask;

	if (copyout(&frame, fp, sizeof frame) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_ds = GSEL(GUDATA32_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA32_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUDATA32_SEL, SEL_UPL);
	tf->tf_gs = GSEL(GUDATA32_SEL, SEL_UPL);

	tf->tf_rip = (u_int64_t)catcher;
	tf->tf_cs = GSEL(GUCODE32_SEL, SEL_UPL);
	tf->tf_rflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_rsp = (u_int64_t)fp;
	tf->tf_ss = GSEL(GUDATA32_SEL, SEL_UPL);

	/* Remember that we're now on the signal stack. */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
}
#endif

static void
netbsd32_sendsig_siginfo(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct sigacts *ps = p->p_sigacts;
	int onstack;
	int sig = ksi->ksi_signo;
	struct netbsd32_sigframe_siginfo *fp, frame;
	sig_t catcher = SIGACTION(p, sig).sa_handler;
	struct trapframe *tf = l->l_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/* Allocate space for the signal handler context. */
	if (onstack)
		fp = (struct netbsd32_sigframe_siginfo *)
		    ((caddr_t)p->p_sigctx.ps_sigstk.ss_sp +
					  p->p_sigctx.ps_sigstk.ss_size);
	else
		fp = (struct netbsd32_sigframe_siginfo *)tf->tf_rsp;

	fp--;

	/* Build stack frame for signal trampoline. */
	switch (ps->sa_sigdesc[sig].sd_vers) {
	case 0:		/* handled by sendsig_sigcontext */
	case 1:		/* handled by sendsig_sigcontext */
	default:	/* unknown version */
		printf("nsendsig: bad version %d\n",
		    ps->sa_sigdesc[sig].sd_vers);
		sigexit(l, SIGILL);
	case 2:
		break;
	}

	frame.sf_ra = (uint32_t)(uintptr_t)ps->sa_sigdesc[sig].sd_tramp;
	frame.sf_signum = sig;
	frame.sf_sip = (uint32_t)(uintptr_t)&fp->sf_si;
	frame.sf_ucp = (uint32_t)(uintptr_t)&fp->sf_uc;
	netbsd32_si_to_si32(&frame.sf_si, (siginfo_t *)&ksi->ksi_info);
	frame.sf_uc.uc_flags = _UC_SIGMASK;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_link = 0;
	frame.sf_uc.uc_flags |= (p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK)
	    ? _UC_SETSTACK : _UC_CLRSTACK;
	memset(&frame.sf_uc.uc_stack, 0, sizeof(frame.sf_uc.uc_stack));
	cpu_getmcontext32(l, &frame.sf_uc.uc_mcontext, &frame.sf_uc.uc_flags);

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_ds = GSEL(GUDATA32_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA32_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUDATA32_SEL, SEL_UPL);
	tf->tf_gs = GSEL(GUDATA32_SEL, SEL_UPL);

	tf->tf_rip = (u_int64_t)catcher;
	tf->tf_cs = GSEL(GUCODE32_SEL, SEL_UPL);
	tf->tf_rflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_rsp = (u_int64_t)fp;
	tf->tf_ss = GSEL(GUDATA32_SEL, SEL_UPL);

	/* Remember that we're now on the signal stack. */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
}

void
netbsd32_sendsig(const ksiginfo_t *ksi, const sigset_t *mask)
{
#ifdef COMPAT_16
	if (curproc->p_sigacts->sa_sigdesc[ksi->ksi_signo].sd_vers < 2)
		netbsd32_sendsig_sigcontext(ksi, mask);
	else
#endif
		netbsd32_sendsig_siginfo(ksi, mask);
}

int
compat_16_netbsd32___sigreturn14(struct lwp *l, void *v, register_t *retval)
{
	struct compat_16_netbsd32___sigreturn14_args /* {
		syscallarg(netbsd32_sigcontextp_t) sigcntxp;
	} */ *uap = v;
	struct netbsd32_sigcontext *scp, context;
	struct trapframe *tf;
	struct proc *p = l->l_proc;
	int error;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = (struct netbsd32_sigcontext *)(uintptr_t)SCARG(uap, sigcntxp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/* Restore register context. */
	tf = l->l_md.md_regs;
	/*
	 * Check for security violations.
	 */
	error = check_sigcontext32(&context, tf);
	if (error != 0)
		return error;

	tf->tf_ds = context.sc_ds;
	tf->tf_es = context.sc_es;
	tf->tf_fs = context.sc_fs;
	tf->tf_gs = context.sc_gs;

	tf->tf_rflags = context.sc_eflags;
	tf->tf_rdi = context.sc_edi;
	tf->tf_rsi = context.sc_esi;
	tf->tf_rbp = context.sc_ebp;
	tf->tf_rbx = context.sc_ebx;
	tf->tf_rdx = context.sc_edx;
	tf->tf_rcx = context.sc_ecx;
	tf->tf_rax = context.sc_eax;

	tf->tf_rip = context.sc_eip;
	tf->tf_cs = context.sc_cs;
	tf->tf_rsp = context.sc_esp;
	tf->tf_ss = context.sc_ss;

	/* Restore signal stack. */
	if (context.sc_onstack & SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	(void) sigprocmask1(p, SIG_SETMASK, &context.sc_mask, 0);

	return (EJUSTRETURN);
}


/*
 * Dump the machine specific segment at the start of a core dump.
 */     
struct md_core32 {
	struct reg32 intreg;
	struct fpreg32 freg;
};

int
cpu_coredump32(struct lwp *l, struct vnode *vp, struct ucred *cred,
	     struct core32 *chdr)
{
	struct proc *p = l->l_proc;
	struct md_core32 md_core;
	struct coreseg cseg;
	int error;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_I386, 0);
	chdr->c_hdrsize = ALIGN32(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN32(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Save integer registers. */
	error = process_read_regs32(l, &md_core.intreg);
	if (error)
		return error;

	/* Save floating point registers. */
	error = process_read_fpregs32(l, &md_core.freg);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_I386, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred,
	    NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}


int
process_read_regs32(struct lwp *l, struct reg32 *regs)
{
	struct trapframe *tf = l->l_md.md_regs;

	regs->r_gs = LSEL(LUCODE32_SEL, SEL_UPL);
	regs->r_fs = LSEL(LUCODE32_SEL, SEL_UPL);
	regs->r_es = LSEL(LUCODE32_SEL, SEL_UPL);
	regs->r_ds = LSEL(LUCODE32_SEL, SEL_UPL);
	regs->r_eflags = tf->tf_rflags;
	/* XXX avoid sign extension problems with unknown upper bits? */
	regs->r_edi = tf->tf_rdi & 0xffffffff;
	regs->r_esi = tf->tf_rsi & 0xffffffff;
	regs->r_ebp = tf->tf_rbp & 0xffffffff;
	regs->r_ebx = tf->tf_rbx & 0xffffffff;
	regs->r_edx = tf->tf_rdx & 0xffffffff;
	regs->r_ecx = tf->tf_rcx & 0xffffffff;
	regs->r_eax = tf->tf_rax & 0xffffffff;
	regs->r_eip = tf->tf_rip & 0xffffffff;
	regs->r_cs = tf->tf_cs;
	regs->r_esp = tf->tf_rsp & 0xffffffff;
	regs->r_ss = tf->tf_ss;

	return (0);
}

int
process_read_fpregs32(struct lwp *l, struct fpreg32 *regs)
{
	struct oldfsave frame;

	if (l->l_md.md_flags & MDP_USEDFPU) {
		fpusave_lwp(l, 1);
	} else {
		memset(&frame, 0, sizeof(*regs));
		frame.fs_control = __NetBSD_NPXCW__;
		frame.fs_tag = 0xffff;
		l->l_md.md_flags |= MDP_USEDFPU;
	}

	memcpy(regs, &frame, sizeof(*regs));
	return (0);
}

int
netbsd32_sysarch(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct netbsd32_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(netbsd32_voidp) parms;
	} */ *uap = v;
	int error;

	switch (SCARG(uap, op)) {
		case X86_64_IOPL:
			error = x86_64_iopl(l,
			    (void *)(uintptr_t)SCARG(uap, parms), retval);
			break;
		case X86_64_GET_MTRR:
			error = x86_64_get_mtrr32(l,
			    (void *)(uintptr_t)SCARG(uap, parms),
			    retval);
			break;
		case X86_64_SET_MTRR:
			error = x86_64_set_mtrr32(l,
			    (void *)(uintptr_t)SCARG(uap, parms),
			    retval);
			break;
		default:
			error = EINVAL;
			break;
	}
	return error;
}

static int
x86_64_get_mtrr32(struct lwp *l, void *args, register_t *retval)
{
	struct x86_64_get_mtrr_args32 args32;
	int error, i;
	int32_t n;
	struct mtrr32 *m32p, m32;
	struct mtrr *m64p, *mp;
	struct proc *p = l->l_proc;

	m64p = NULL;

	if (mtrr_funcs == NULL)
		return ENOSYS;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error != 0)
		return error;

	error = copyin(args, &args32, sizeof args32);
	if (error != 0)
		return error;

	if (args32.mtrrp == 0) {
		n = (MTRR_I686_NFIXED_SOFT + MTRR_I686_NVAR);
		return copyout(&n, (void *)(uintptr_t)args32.n, sizeof n);
	}

	error = copyin((void *)(uintptr_t)args32.n, &n, sizeof n);
	if (error != 0)
		return error;

	if (n <= 0 || n > (MTRR_I686_NFIXED_SOFT + MTRR_I686_NVAR))
		return EINVAL;

	m64p = malloc(n * sizeof (struct mtrr), M_TEMP, M_WAITOK);
	if (m64p == NULL) {
		error = ENOMEM;
		goto fail;
	}
	error = mtrr_get(m64p, &n, p, 0);
	if (error != 0)
		goto fail;
	m32p = (struct mtrr32 *)(uintptr_t)args32.mtrrp;
	mp = m64p;
	for (i = 0; i < n; i++) {
		m32.base = mp->base;
		m32.len = mp->len;
		m32.type = mp->type;
		m32.flags = mp->flags;
		m32.owner = mp->owner;
		error = copyout(&m32, m32p, sizeof m32);
		if (error != 0)
			break;
		mp++;
		m32p++;
	}
fail:
	if (m64p != NULL)
		free(m64p, M_TEMP);
	if (error != 0)
		n = 0;
	copyout(&n, (void *)(uintptr_t)args32.n, sizeof n);
	return error;
		
}

static int
x86_64_set_mtrr32(struct lwp *l, void *args, register_t *retval)
{
	struct x86_64_set_mtrr_args32 args32;
	struct mtrr32 *m32p, m32;
	struct mtrr *m64p, *mp;
	int error, i;
	int32_t n;
	struct proc *p = l->l_proc;

	m64p = NULL;

	if (mtrr_funcs == NULL)
		return ENOSYS;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error != 0)
		return error;

	error = copyin(args, &args32, sizeof args32);
	if (error != 0)
		return error;

	error = copyin((void *)(uintptr_t)args32.n, &n, sizeof n);
	if (error != 0)
		return error;

	if (n <= 0 || n > (MTRR_I686_NFIXED_SOFT + MTRR_I686_NVAR)) {
		error = EINVAL;
		goto fail;
	}

	m64p = malloc(n * sizeof (struct mtrr), M_TEMP, M_WAITOK);
	if (m64p == NULL) {
		error = ENOMEM;
		goto fail;
	}
	m32p = (struct mtrr32 *)(uintptr_t)args32.mtrrp;
	mp = m64p;
	for (i = 0; i < n; i++) {
		error = copyin(m32p, &m32, sizeof m32);
		if (error != 0)
			goto fail;
		mp->base = m32.base;
		mp->len = m32.len;
		mp->type = m32.type;
		mp->flags = m32.flags;
		mp->owner = m32.owner;
		m32p++;
		mp++;
	}

	error = mtrr_set(m64p, &n, p, 0);
fail:
	if (m64p != NULL)
		free(m64p, M_TEMP);
	if (error != 0)
		n = 0;
	copyout(&n, (void *)(uintptr_t)args32.n, sizeof n);
	return error;
}

#if 0
void
netbsd32_mcontext_to_mcontext32(mcontext32_t *m32, mcontext_t *m, int flags)
{
	if ((flags & _UC_CPU) != 0) {
		m32->__gregs[_REG32_GS] = m->__gregs[_REG_GS] & 0xffffffff;
		m32->__gregs[_REG32_FS] = m->__gregs[_REG_FS] & 0xffffffff;
		m32->__gregs[_REG32_ES] = m->__gregs[_REG_ES] & 0xffffffff;
		m32->__gregs[_REG32_DS] = m->__gregs[_REG_DS] & 0xffffffff;
		m32->__gregs[_REG32_EDI] = m->__gregs[_REG_RDI] & 0xffffffff;
		m32->__gregs[_REG32_ESI] = m->__gregs[_REG_RSI] & 0xffffffff;
		m32->__gregs[_REG32_EBP] = m->__gregs[_REG_RBP] & 0xffffffff;
		m32->__gregs[_REG32_ESP] = m->__gregs[_REG_URSP] & 0xffffffff;
		m32->__gregs[_REG32_EBX] = m->__gregs[_REG_RBX] & 0xffffffff;
		m32->__gregs[_REG32_EDX] = m->__gregs[_REG_RDX] & 0xffffffff;
		m32->__gregs[_REG32_ECX] = m->__gregs[_REG_RCX] & 0xffffffff;
		m32->__gregs[_REG32_EAX] = m->__gregs[_REG_RAX] & 0xffffffff;
		m32->__gregs[_REG32_TRAPNO] =
		    m->__gregs[_REG_TRAPNO] & 0xffffffff;
		m32->__gregs[_REG32_ERR] = m->__gregs[_REG_ERR] & 0xffffffff;
		m32->__gregs[_REG32_EIP] = m->__gregs[_REG_RIP] & 0xffffffff;
		m32->__gregs[_REG32_CS] = m->__gregs[_REG_CS] & 0xffffffff;
		m32->__gregs[_REG32_EFL] = m->__gregs[_REG_RFL] & 0xffffffff;
		m32->__gregs[_REG32_UESP] = m->__gregs[_REG_URSP] & 0xffffffff;
		m32->__gregs[_REG32_SS] = m->__gregs[_REG_SS] & 0xffffffff;
	}
	if ((flags & _UC_FPU) != 0)
		memcpy(&m32->__fpregs, &m->__fpregs, sizeof (m32->__fpregs));
}

void
netbsd32_mcontext32_to_mcontext(mcontext_t *m, mcontext32_t *m32, int flags)
{
	if ((flags & _UC_CPU) != 0) {
		m->__gregs[_REG_GS] = m32->__gregs[_REG32_GS];
		m->__gregs[_REG_FS] = m32->__gregs[_REG32_FS];
		m->__gregs[_REG_ES] = m32->__gregs[_REG32_ES];
		m->__gregs[_REG_DS] = m32->__gregs[_REG32_DS];
		m->__gregs[_REG_RDI] = m32->__gregs[_REG32_EDI];
		m->__gregs[_REG_RSI] = m32->__gregs[_REG32_ESI];
		m->__gregs[_REG_RBP] = m32->__gregs[_REG32_EBP];
		m->__gregs[_REG_URSP] = m32->__gregs[_REG32_ESP];
		m->__gregs[_REG_RBX] = m32->__gregs[_REG32_EBX];
		m->__gregs[_REG_RDX] = m32->__gregs[_REG32_EDX];
		m->__gregs[_REG_RCX] = m32->__gregs[_REG32_ECX];
		m->__gregs[_REG_RAX] = m32->__gregs[_REG32_EAX];
		m->__gregs[_REG_TRAPNO] = m32->__gregs[_REG32_TRAPNO];
		m->__gregs[_REG_ERR] = m32->__gregs[_REG32_ERR];
		m->__gregs[_REG_RIP] = m32->__gregs[_REG32_EIP];
		m->__gregs[_REG_CS] = m32->__gregs[_REG32_CS];
		m->__gregs[_REG_RFL] = m32->__gregs[_REG32_EFL];
		m->__gregs[_REG_URSP] = m32->__gregs[_REG32_UESP];
		m->__gregs[_REG_SS] = m32->__gregs[_REG32_SS];
	}
	if (flags & _UC_FPU)
		memcpy(&m->__fpregs, &m32->__fpregs, sizeof (m->__fpregs));
}
#endif


int
cpu_setmcontext32(struct lwp *l, const mcontext32_t *mcp, unsigned int flags)
{
	struct trapframe *tf = l->l_md.md_regs;
	__greg32_t *gr = mcp->__gregs;
	int error;

	/* Restore register context, if any. */
	if ((flags & _UC_CPU) != 0) {
		/*
		 * Check for security violations.
		 */
		error = check_mcontext32(mcp, tf);
		if (error != 0)
			return error;
		tf->tf_gs = gr[_REG32_GS];
		tf->tf_fs = gr[_REG32_FS];
		tf->tf_es = gr[_REG32_ES];
		tf->tf_ds = gr[_REG32_DS];
		/* Only change the user-alterable part of eflags */
		tf->tf_rflags &= ~PSL_USER;
		tf->tf_rflags |= (gr[_REG32_EFL] & PSL_USER);
		tf->tf_rdi    = gr[_REG32_EDI];
		tf->tf_rsi    = gr[_REG32_ESI];
		tf->tf_rbp    = gr[_REG32_EBP];
		tf->tf_rbx    = gr[_REG32_EBX];
		tf->tf_rdx    = gr[_REG32_EDX];
		tf->tf_rcx    = gr[_REG32_ECX];
		tf->tf_rax    = gr[_REG32_EAX];
		tf->tf_rip    = gr[_REG32_EIP];
		tf->tf_cs     = gr[_REG32_CS];
		tf->tf_rsp    = gr[_REG32_UESP];
		tf->tf_ss     = gr[_REG32_SS];
	}

	/* Restore floating point register context, if any. */
	if ((flags & _UC_FPU) != 0) {
		/*
		 * If we were using the FPU, forget that we were.
		 */
		if (l->l_addr->u_pcb.pcb_fpcpu != NULL)
			fpusave_lwp(l, 0);
		memcpy(&l->l_addr->u_pcb.pcb_savefpu.fp_fxsave, &mcp->__fpregs,
		    sizeof (mcp->__fpregs));
		/* If not set already. */
		l->l_md.md_flags |= MDP_USEDFPU;
	}
	if (flags & _UC_SETSTACK)
		l->l_proc->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	if (flags & _UC_CLRSTACK)
		l->l_proc->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;
	return (0);
}

void
cpu_getmcontext32(struct lwp *l, mcontext32_t *mcp, unsigned int *flags)
{
	const struct trapframe *tf = l->l_md.md_regs;
	__greg32_t *gr = mcp->__gregs;
	__greg32_t ras_eip;

	/* Save register context. */
	gr[_REG32_GS]  = tf->tf_gs;
	gr[_REG32_FS]  = tf->tf_fs;
	gr[_REG32_ES]  = tf->tf_es;
	gr[_REG32_DS]  = tf->tf_ds;
	gr[_REG32_EFL] = tf->tf_rflags;
	gr[_REG32_EDI]    = tf->tf_rdi;
	gr[_REG32_ESI]    = tf->tf_rsi;
	gr[_REG32_EBP]    = tf->tf_rbp;
	gr[_REG32_EBX]    = tf->tf_rbx;
	gr[_REG32_EDX]    = tf->tf_rdx;
	gr[_REG32_ECX]    = tf->tf_rcx;
	gr[_REG32_EAX]    = tf->tf_rax;
	gr[_REG32_EIP]    = tf->tf_rip;
	gr[_REG32_CS]     = tf->tf_cs;
	gr[_REG32_ESP]    = tf->tf_rsp;
	gr[_REG32_UESP]   = tf->tf_rsp;
	gr[_REG32_SS]     = tf->tf_ss;
	gr[_REG32_TRAPNO] = tf->tf_trapno;
	gr[_REG32_ERR]    = tf->tf_err;

	if ((ras_eip = (__greg32_t)(uintptr_t)ras_lookup(l->l_proc,
	    (caddr_t) (uintptr_t)gr[_REG32_EIP])) != -1)
		gr[_REG32_EIP] = ras_eip;

	*flags |= _UC_CPU;

	/* Save floating point register context, if any. */
	if ((l->l_md.md_flags & MDP_USEDFPU) != 0) {
		if (l->l_addr->u_pcb.pcb_fpcpu)
			fpusave_lwp(l, 1);
		memcpy(&mcp->__fpregs, &l->l_addr->u_pcb.pcb_savefpu.fp_fxsave,
		    sizeof (mcp->__fpregs));
		*flags |= _UC_FPU;
	}
}

/*
 * For various reasons, the amd64 port can't do what the i386 port does,
 * and rely on catching invalid user contexts on exit from the kernel.
 * These functions perform the needed checks.
 */
static int
check_sigcontext32(const struct netbsd32_sigcontext *scp, struct trapframe *tf)
{
	if (((scp->sc_eflags ^ tf->tf_rflags) & PSL_USERSTATIC) != 0)
		return EINVAL;
	if (scp->sc_fs != 0 && !VALID_USER_DSEL32(scp->sc_fs))
		return EINVAL;
	if (scp->sc_gs != 0 && !VALID_USER_DSEL32(scp->sc_gs))
		return EINVAL;
	if (scp->sc_es != 0 && !VALID_USER_DSEL32(scp->sc_es))
		return EINVAL;
	if (!VALID_USER_DSEL32(scp->sc_ds) || !VALID_USER_DSEL32(scp->sc_ss))
		return EINVAL;
	if (scp->sc_eip >= VM_MAXUSER_ADDRESS32)
		return EINVAL;
	return 0;
}

static int
check_mcontext32(const mcontext32_t *mcp, struct trapframe *tf)
{
	const __greg32_t *gr;

	gr = mcp->__gregs;

	if (((gr[_REG32_EFL] ^ tf->tf_rflags) & PSL_USERSTATIC) != 0)
		return EINVAL;
	if (gr[_REG32_FS] != 0 && !VALID_USER_DSEL32(gr[_REG32_FS]))
		return EINVAL;
	if (gr[_REG32_GS] != 0 && !VALID_USER_DSEL32(gr[_REG32_GS]))
		return EINVAL;
	if (gr[_REG32_ES] != 0 && !VALID_USER_DSEL32(gr[_REG32_ES]))
		return EINVAL;
	if (!VALID_USER_DSEL32(gr[_REG32_DS]) ||
	    !VALID_USER_DSEL32(gr[_REG32_SS]))
		return EINVAL;
	if (gr[_REG32_EIP] >= VM_MAXUSER_ADDRESS32)
		return EINVAL;
	return 0;
}
