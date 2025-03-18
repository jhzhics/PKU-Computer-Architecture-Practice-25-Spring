// modified from https://github.com/westerndigitalcorporation/RISC-V-Linux/blob/master/riscv-pk/pk/syscall.h by jhzhics, 3/18/2025

#ifndef _RV_SYSCALL_H
#define _RV_SYSCALL_H

#define SYS_RV64_exit 93
#define SYS_RV64_exit_group 94
#define SYS_RV64_getpid 172
#define SYS_RV64_kill 129
#define SYS_RV64_read 63
#define SYS_RV64_write 64
#define SYS_RV64_openat 56
#define SYS_RV64_close 57
#define SYS_RV64_lseek 62
#define SYS_RV64_brk 214
#define SYS_RV64_linkat 37
#define SYS_RV64_unlinkat 35
#define SYS_RV64_mkdirat 34
#define SYS_RV64_renameat 38
#define SYS_RV64_chdir 49
#define SYS_RV64_getcwd 17
#define SYS_RV64_fstat 80
#define SYS_RV64_fstatat 79
#define SYS_RV64_faccessat 48
#define SYS_RV64_pread 67
#define SYS_RV64_pwrite 68
#define SYS_RV64_uname 160
#define SYS_RV64_getuid 174
#define SYS_RV64_geteuid 175
#define SYS_RV64_getgid 176
#define SYS_RV64_getegid 177
#define SYS_RV64_mmap 222
#define SYS_RV64_munmap 215
#define SYS_RV64_mremap 216
#define SYS_RV64_mprotect 226
#define SYS_RV64_prlimit64 261
#define SYS_RV64_getmainvars 2011
#define SYS_RV64_rt_sigaction 134
#define SYS_RV64_writev 66
#define SYS_RV64_gettimeofday 169
#define SYS_RV64_times 153
#define SYS_RV64_fcntl 25
#define SYS_RV64_ftruncate 46
#define SYS_RV64_getdents 61
#define SYS_RV64_dup 23
#define SYS_RV64_readlinkat 78
#define SYS_RV64_rt_sigprocmask 135
#define SYS_RV64_ioctl 29
#define SYS_RV64_getrlimit 163
#define SYS_RV64_setrlimit 164
#define SYS_RV64_getrusage 165
#define SYS_RV64_clock_gettime 113
#define SYS_RV64_set_tid_address 96
#define SYS_RV64_RV64_set_robust_list 99
#define SYS_RV64_open 1024
#define SYS_RV64_link 1025
#define SYS_RV64_unlink 1026
#define SYS_RV64_mkdir 1030
#define SYS_RV64_access 1033
#define SYS_RV64_stat 1038
#define SYS_RV64_lstat 1039
#define SYS_RV64_time 1062
#endif