/* $Id$ */
/* Copyright Gerhard Rieger 2007 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source of the internal xiosocketpair function */

#include "xiosysincludes.h"
#include "sycls.h"
#include "error.h"
#include "xio.h"

#if defined(HAVE_DEV_PTMX)
#  define PTMX "/dev/ptmx"	/* Linux */
#elif HAVE_DEV_PTC
#  define PTMX "/dev/ptc"	/* AIX */
#endif

#define MAXPTYNAMELEN 64

/* how: 0...socketpair; 1...pipes pair; 2...pty (master, slave)
   how==0: var args (int)domain, (int)type, (int)protocol
   how==1: no var args
   how==2: var args (int)useptmx
   returns -1 on error or 0 on success */

int xiosocketpair2(xiofile_t **xfd1p, xiofile_t **xfd2p, int how, ...) {
   va_list ap;
   xiofile_t *xfd1, *xfd2;
   int result = 0;

   if ((xfd1 = xioallocfd()) == NULL) {
      return -1;
   }
   if ((xfd2 = xioallocfd()) == NULL) {
      xiofreefd(xfd1);
      return -1;
   }

   switch (how) {
   case 0:	/* socketpair */
      {
	 int sv[2];
	 int domain, type, protocol;

	 va_start(ap, how);
	 domain   = va_arg(ap, int);
	 type     = va_arg(ap, int);
	 protocol = va_arg(ap, int);
	 va_end(ap);
	 if (Socketpair(domain, type, protocol, sv) < 0) {
	    Error5("socketpair(%d, %d, %d, %p): %s",
		   domain, type, protocol, sv, strerror(errno));
	    xiofreefd(xfd1);  xiofreefd(xfd2);
	    return -1;
	 }
	 assert(xfd1->stream.fdtype == FDTYPE_SINGLE);
	 xfd1->stream.fd1 = sv[0];
	 assert(xfd2->stream.fdtype == FDTYPE_SINGLE);
	 xfd2->stream.fd1 = sv[1];
      }
      break;

   case 1:
      {
	 int filedes1[2], filedes2[2];
	 if (Pipe(filedes1) < 0) {
	    Error2("pipe(%p): %s", filedes1, strerror(errno));
	    xiofreefd(xfd1);  xiofreefd(xfd2);
	    return -1;
	 }
	 if (Pipe(filedes2) < 0) {
	    Error2("pipe(%p): %s", filedes2, strerror(errno));
	    xiofreefd(xfd1);  xiofreefd(xfd2);
	    Close(filedes1[0]);  Close(filedes1[1]);
	    return -1;
	 }	
	 xfd1->stream.fd1 = filedes1[0];
	 xfd1->stream.fd2 = filedes2[1];
	 xfd1->stream.fdtype = FDTYPE_DOUBLE;
	 xfd1->stream.dtype = XIODATA_2PIPE;
	 xfd2->stream.fd1 = filedes2[0];
	 xfd2->stream.fd2 = filedes1[1];
	 xfd2->stream.fdtype = FDTYPE_DOUBLE;
	 xfd2->stream.dtype = XIODATA_2PIPE;
      }
      break;

#if HAVE_DEV_PTMX || HAVE_DEV_PTC
   case 2:	/* pty (master, slave) */
      {
	 int useptmx;
	 char ptyname[MAXPTYNAMELEN];
	 int ptyfd = -1, ttyfd;

	 va_start(ap, how);
	 useptmx = va_arg(ap, int);
	 va_end(ap);

	 if (useptmx) {
	    if ((ptyfd = Open(PTMX, O_RDWR|O_NOCTTY, 0620)) < 0) {
	       Warn1("open(\""PTMX"\", O_RDWR|O_NOCTTY, 0620): %s",
		     strerror(errno));
	       /*!*/
	    } else {
	       ;/*0 Info1("open(\""PTMX"\", O_RDWR|O_NOCTTY, 0620) -> %d", ptyfd);*/
	    }
	    if (ptyfd >= 0) {
	       char *tn = NULL;

	       /* we used PTMX before forking */
	       /*0 extern char *ptsname(int);*/
#if HAVE_GRANTPT	/* AIX, not Linux */
	       if (Grantpt(ptyfd)/*!*/ < 0) {
		  Warn2("grantpt(%d): %s", ptyfd, strerror(errno));
	       }
#endif /* HAVE_GRANTPT */
#if HAVE_UNLOCKPT
	       if (Unlockpt(ptyfd)/*!*/ < 0) {
		  Warn2("unlockpt(%d): %s", ptyfd, strerror(errno));
	       }
#endif /* HAVE_UNLOCKPT */
#if HAVE_PTSNAME	/* AIX, not Linux */
	       if ((tn = Ptsname(ptyfd)) == NULL) {
		  Warn2("ptsname(%d): %s", ptyfd, strerror(errno));
	       } else {
		  Notice1("PTY is %s", tn);
	       }
#endif /* HAVE_PTSNAME */
#if 0
	       if (tn == NULL) {
		  /*! ttyname_r() */
		  if ((tn = Ttyname(ptyfd)) == NULL) {
		     Warn2("ttyname(%d): %s", ptyfd, strerror(errno));
		  }
	       }
	       strncpy(ptyname, tn, MAXPTYNAMELEN);
#endif
	       if ((ttyfd = Open(tn, O_RDWR|O_NOCTTY, 0620)) < 0) {
		  Warn2("open(\"%s\", O_RDWR|O_NOCTTY, 0620): %s", tn, strerror(errno));
	       } else {
		  /*0 Info2("open(\"%s\", O_RDWR|O_NOCTTY, 0620) -> %d", tn, ttyfd);*/
	       }

#ifdef I_PUSH
	       /* Linux: I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> -1 EINVAL */
	       /* AIX:   I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> 1 */
	       /* SunOS: I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> 0 */
	       /* HP-UX: I_PUSH def'd; pty: ioctl(, I_FIND, ...) -> 0 */
	       if (Ioctl(ttyfd, I_FIND, "ldterm") == 0) {
		  Ioctl(ttyfd, I_PUSH, "ptem");		/* 0 */
		  Ioctl(ttyfd, I_PUSH, "ldterm");		/* 0 */
		  Ioctl(ttyfd, I_PUSH, "ttcompat");	/* HP-UX: -1 */
	       }
#endif
	    }
	 }
#if HAVE_OPENPTY
	 if (ptyfd < 0) {
	    int result;
	    if ((result = Openpty(&ptyfd, &ttyfd, ptyname, NULL, NULL)) < 0) {
	       Error4("openpty(%p, %p, %p, NULL, NULL): %s",
		      &ptyfd, &ttyfd, ptyname, strerror(errno));
	       return -1;
	    }
	    Notice1("PTY is %s", ptyname);
	 }
#endif /* HAVE_OPENPTY */
	 assert(xfd1->stream.fdtype == FDTYPE_SINGLE);
	 xfd1->stream.fd1 = ttyfd;
	 assert(xfd2->stream.fdtype == FDTYPE_SINGLE);
	 xfd2->stream.fd1 = ptyfd;
      }
      break;
#endif /* HAVE_DEV_PTMX || HAVE_DEV_PTC */

   default:
      Error1("undefined socketpair mechanism %d", how);
      xiofreefd(xfd1);  xiofreefd(xfd2);
      return -1;
   }

   *xfd1p = xfd1;
   *xfd2p = xfd2;
   return 0;
}

