/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $FreeBSD: src/sys/alpha/linux/linux_syscall.h,v 1.16.2.2 2005/03/31 22:24:24 sobomax Exp $
 * created from FreeBSD: src/sys/alpha/linux/syscalls.master,v 1.58.2.2 2005/03/31 22:17:42 sobomax Exp 
 */

#define	LINUX_SYS_exit	1
#define	LINUX_SYS_linux_fork	2
#define	LINUX_SYS_read	3
#define	LINUX_SYS_write	4
#define	LINUX_SYS_close	6
#define	LINUX_SYS_osf1_wait4	7
#define	LINUX_SYS_linux_link	9
#define	LINUX_SYS_linux_unlink	10
#define	LINUX_SYS_linux_chdir	12
#define	LINUX_SYS_fchdir	13
#define	LINUX_SYS_linux_mknod	14
#define	LINUX_SYS_linux_chmod	15
#define	LINUX_SYS_linux_chown	16
#define	LINUX_SYS_linux_brk	17
#define	LINUX_SYS_linux_lseek	19
#define	LINUX_SYS_getpid	20
#define	LINUX_SYS_linux_umount	22
#define	LINUX_SYS_setuid	23
#define	LINUX_SYS_getuid	24
#define	LINUX_SYS_linux_ptrace	26
#define	LINUX_SYS_linux_access	33
#define	LINUX_SYS_sync	36
#define	LINUX_SYS_linux_kill	37
#define	LINUX_SYS_setpgid	39
#define	LINUX_SYS_dup	41
#define	LINUX_SYS_pipe	42
#define	LINUX_SYS_linux_open	45
#define	LINUX_SYS_getgid	47
#define	LINUX_SYS_osf1_sigprocmask	48
#define	LINUX_SYS_acct	51
#define	LINUX_SYS_linux_sigpending	52
#define	LINUX_SYS_linux_ioctl	54
#define	LINUX_SYS_linux_symlink	57
#define	LINUX_SYS_linux_readlink	58
#define	LINUX_SYS_linux_execve	59
#define	LINUX_SYS_umask	60
#define	LINUX_SYS_chroot	61
#define	LINUX_SYS_getpgrp	63
#define	LINUX_SYS_linux_getpagesize	64
#define	LINUX_SYS_linux_vfork	66
#define	LINUX_SYS_linux_newstat	67
#define	LINUX_SYS_linux_newlstat	68
#define	LINUX_SYS_linux_mmap	71
#define	LINUX_SYS_linux_munmap	73
#define	LINUX_SYS_linux_mprotect	74
#define	LINUX_SYS_madvise	75
#define	LINUX_SYS_linux_vhangup	76
#define	LINUX_SYS_linux_setgroups	79
#define	LINUX_SYS_linux_getgroups	80
#define	LINUX_SYS_osf1_setitimer	83
#define	LINUX_SYS_linux_gethostname	87
#define	LINUX_SYS_osethostname	88
#define	LINUX_SYS_linux_getdtablesize	89
#define	LINUX_SYS_dup2	90
#define	LINUX_SYS_linux_newfstat	91
#define	LINUX_SYS_linux_fcntl	92
#define	LINUX_SYS_osf1_select	93
#define	LINUX_SYS_poll	94
#define	LINUX_SYS_fsync	95
#define	LINUX_SYS_setpriority	96
#define	LINUX_SYS_osf1_socket	97
#define	LINUX_SYS_linux_connect	98
#define	LINUX_SYS_accept	99
#define	LINUX_SYS_osend	101
#define	LINUX_SYS_orecv	102
#define	LINUX_SYS_osf1_sigreturn	103
#define	LINUX_SYS_bind	104
#define	LINUX_SYS_setsockopt	105
#define	LINUX_SYS_listen	106
#define	LINUX_SYS_osf1_sigsuspend	111
#define	LINUX_SYS_linux_recvmsg	113
#define	LINUX_SYS_linux_sendmsg	114
#define	LINUX_SYS_osf1_gettimeofday	116
#define	LINUX_SYS_osf1_getrusage	117
#define	LINUX_SYS_getsockopt	118
#define	LINUX_SYS_readv	120
#define	LINUX_SYS_writev	121
#define	LINUX_SYS_fchown	123
#define	LINUX_SYS_fchmod	124
#define	LINUX_SYS_recvfrom	125
#define	LINUX_SYS_setreuid	126
#define	LINUX_SYS_setregid	127
#define	LINUX_SYS_linux_rename	128
#define	LINUX_SYS_linux_truncate	129
#define	LINUX_SYS_oftruncate	130
#define	LINUX_SYS_flock	131
#define	LINUX_SYS_setgid	132
#define	LINUX_SYS_osf1_sendto	133
#define	LINUX_SYS_shutdown	134
#define	LINUX_SYS_linux_socketpair	135
#define	LINUX_SYS_linux_mkdir	136
#define	LINUX_SYS_linux_rmdir	137
#define	LINUX_SYS_utimes	138
#define	LINUX_SYS_ogetpeername	141
#define	LINUX_SYS_linux_getrlimit	144
#define	LINUX_SYS_linux_setrlimit	145
#define	LINUX_SYS_setsid	147
#define	LINUX_SYS_linux_quotactl	148
#define	LINUX_SYS_getsockname	150
#define	LINUX_SYS_osf1_sigaction	156
#define	LINUX_SYS_setdomainname	166
#define	LINUX_SYS_linux_msgctl	200
#define	LINUX_SYS_linux_msgget	201
#define	LINUX_SYS_linux_msgrcv	202
#define	LINUX_SYS_linux_msgsnd	203
#define	LINUX_SYS_linux_semctl	204
#define	LINUX_SYS_linux_semget	205
#define	LINUX_SYS_linux_semop	206
#define	LINUX_SYS_linux_lchown	208
#define	LINUX_SYS_linux_shmat	209
#define	LINUX_SYS_linux_shmctl	210
#define	LINUX_SYS_linux_shmdt	211
#define	LINUX_SYS_linux_shmget	212
#define	LINUX_SYS_linux_msync	217
#define	LINUX_SYS_getpgid	233
#define	LINUX_SYS_linux_getsid	234
#define	LINUX_SYS_linux_sigaltstack	235
#define	LINUX_SYS_osf1_sysinfo	241
#define	LINUX_SYS_linux_sysfs	254
#define	LINUX_SYS_osf1_getsysinfo	256
#define	LINUX_SYS_osf1_setsysinfo	257
#define	LINUX_SYS_linux_bdflush	300
#define	LINUX_SYS_linux_sethae	301
#define	LINUX_SYS_linux_mount	302
#define	LINUX_SYS_linux_old_adjtimex	303
#define	LINUX_SYS_linux_swapoff	304
#define	LINUX_SYS_linux_getdents	305
#define	LINUX_SYS_linux_create_module	306
#define	LINUX_SYS_linux_init_module	307
#define	LINUX_SYS_linux_delete_module	308
#define	LINUX_SYS_linux_get_kernel_syms	309
#define	LINUX_SYS_linux_syslog	310
#define	LINUX_SYS_linux_reboot	311
#define	LINUX_SYS_linux_clone	312
#define	LINUX_SYS_linux_uselib	313
#define	LINUX_SYS_mlock	314
#define	LINUX_SYS_munlock	315
#define	LINUX_SYS_mlockall	316
#define	LINUX_SYS_munlockall	317
#define	LINUX_SYS_linux_sysinfo	318
#define	LINUX_SYS_linux_sysctl	319
#define	LINUX_SYS_linux_oldumount	321
#define	LINUX_SYS_swapon	322
#define	LINUX_SYS_linux_times	323
#define	LINUX_SYS_linux_personality	324
#define	LINUX_SYS_linux_setfsuid	325
#define	LINUX_SYS_linux_setfsgid	326
#define	LINUX_SYS_linux_ustat	327
#define	LINUX_SYS_linux_statfs	328
#define	LINUX_SYS_linux_fstatfs	329
#define	LINUX_SYS_sched_setparam	330
#define	LINUX_SYS_sched_getparam	331
#define	LINUX_SYS_linux_sched_setscheduler	332
#define	LINUX_SYS_linux_sched_getscheduler	333
#define	LINUX_SYS_sched_yield	334
#define	LINUX_SYS_linux_sched_get_priority_max	335
#define	LINUX_SYS_linux_sched_get_priority_min	336
#define	LINUX_SYS_sched_rr_get_interval	337
#define	LINUX_SYS_linux_newuname	339
#define	LINUX_SYS_nanosleep	340
#define	LINUX_SYS_linux_mremap	341
#define	LINUX_SYS_linux_nfsservctl	342
#define	LINUX_SYS_setresuid	343
#define	LINUX_SYS_getresuid	344
#define	LINUX_SYS_linux_pciconfig_read	345
#define	LINUX_SYS_linux_pciconfig_write	346
#define	LINUX_SYS_linux_query_module	347
#define	LINUX_SYS_linux_prctl	348
#define	LINUX_SYS_linux_pread	349
#define	LINUX_SYS_linux_pwrite	350
#define	LINUX_SYS_linux_rt_sigreturn	351
#define	LINUX_SYS_linux_rt_sigaction	352
#define	LINUX_SYS_linux_rt_sigprocmask	353
#define	LINUX_SYS_linux_rt_sigpending	354
#define	LINUX_SYS_linux_rt_sigtimedwait	355
#define	LINUX_SYS_linux_rt_sigqueueinfo	356
#define	LINUX_SYS_linux_rt_sigsuspend	357
#define	LINUX_SYS_linux_select	358
#define	LINUX_SYS_gettimeofday	359
#define	LINUX_SYS_settimeofday	360
#define	LINUX_SYS_linux_getitimer	361
#define	LINUX_SYS_linux_setitimer	362
#define	LINUX_SYS_linux_utimes	363
#define	LINUX_SYS_getrusage	364
#define	LINUX_SYS_linux_wait4	365
#define	LINUX_SYS_linux_adjtimex	366
#define	LINUX_SYS_linux_getcwd	367
#define	LINUX_SYS_linux_capget	368
#define	LINUX_SYS_linux_capset	369
#define	LINUX_SYS_linux_sendfile	370
#define	LINUX_SYS_setresgid	371
#define	LINUX_SYS_getresgid	372
#define	LINUX_SYS_linux_pivot_root	374
#define	LINUX_SYS_linux_mincore	375
#define	LINUX_SYS_linux_pciconfig_iobase	376
#define	LINUX_SYS_linux_getdents64	377
#define	LINUX_SYS_MAXSYSCALL	378
