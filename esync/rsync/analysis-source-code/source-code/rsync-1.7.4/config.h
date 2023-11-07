/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* compiler specifics */
/* #undef const */
/* #undef inline */
/* #undef HAVE_INLINE */

/* defines for basic types */
/* #undef gid_t */
/* #undef mode_t */
/* #undef off_t */
/* #undef pid_t */
/* #undef size_t */
/* #undef uid_t */

/* The number of bytes in some types  */
#define SIZEOF_LONG 8
#define SIZEOF_INT 4
#define SIZEOF_SHORT 2

/* defines for header files */
#define HAVE_SYS_WAIT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_FCNTL_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_PARAM_H 1
#define TIME_WITH_SYS_TIME 1
#define HAVE_DIRENT_H 1
/* #undef HAVE_MALLOC_H */
/* #undef HAVE_SYS_DIR_H */
#define HAVE_SYS_TIME_H 1
/* #undef HAVE_SYS_TIMES_H */
#define HAVE_UNISTD_H 1
#define HAVE_GRP_H 1
#define HAVE_CTYPE_H 1
/* #undef HAVE_SYS_FILIO_H */
#define HAVE_SYS_IOCTL_H 1
#define HAVE_UTIME_H 1
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_SYS_SOCKET_H 1
/* #undef HAVE_SYS_MODE_H */

/* specific functions */
#define HAVE_FCHMOD 1
#define HAVE_CHMOD 1
#define HAVE_MKNOD 1
#define HAVE_FSTAT 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRTOK 1
#define HAVE_WAITPID 1
#define HAVE_BCOPY 1
#define HAVE_BZERO 1
#define HAVE_READLINK 1
#define HAVE_LINK 1
#define HAVE_UTIME 1
#define HAVE_UTIMES 1
#define HAVE_GETOPT_LONG 1
#define HAVE_FNMATCH 1
#define HAVE_LONGLONG 1
#define HAVE_UTIMBUF 1
#define HAVE_MEMMOVE 1
#define HAVE_MMAP 1
#define HAVE_LCHOWN 1
#define HAVE_SETLINEBUF 1
#define HAVE_GETCWD 1

/* specific programs */
#define HAVE_REMSH 0

#ifndef HAVE_MEMMOVE
#define memmove(d,s,n) bcopy(s,d,n)
#endif


/* for signal declarations */
#define RETSIGTYPE void

/* needed for mknod */
#define HAVE_ST_RDEV 1

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* pgrp info */
/* #undef GETPGRP_VOID */
/* #undef SETPGRP_VOID */

/* Define if you can safely include both <sys/time.h> and <time.h>.  */

/* HP/UX source */
/* #undef _HPUX_SOURCE */

/* Use the "union wait" union to get process status from wait3/waitpid */
/* #undef HAVE_UNION_WAIT */

/* Define if <errno.h> contains a declaration for extern int errno */
#define HAVE_ERRNO_DECL 1

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

