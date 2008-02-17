/* $Id: xio-system.c,v 1.13.2.1 2006/07/24 19:18:36 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2007 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening addresses of system type */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-progcall.h"
#include "xio-system.h"


#if WITH_SYSTEM

static int xioopen_system(int arg, const char *argv[], struct opt *opts,
		int xioflags,	/* XIO_RDONLY etc. */
		xiofile_t *fd,
		unsigned groups,
		int dummy1, int dummy2, int dummy3
		);

static const struct xioaddr_endpoint_desc xioendpoint_system1 = { XIOADDR_SYS, "system", 1, XIOBIT_ALL, GROUP_FD|GROUP_FORK|GROUP_EXEC|GROUP_SOCKET|GROUP_SOCK_UNIX|GROUP_TERMIOS|GROUP_FIFO|GROUP_PTY|GROUP_PARENT, XIOSHUT_UNSPEC, XIOCLOSE_UNSPEC, xioopen_system, 1, 0, 0 HELP(":<shell-command>") };

const union xioaddr_desc *xioaddrs_system[] = {
   (union xioaddr_desc *)&xioendpoint_system1,
   NULL
};

static int xioopen_system(int argc, const char *argv[], struct opt *opts,
		int xioflags,	/* XIO_RDONLY etc. */
		xiofile_t *fd,
		unsigned groups,
		int dummy1, int dummy2, int dummy3
		) {
   int status;
   char *path = NULL;
   int result;
   const char *string = argv[1];

   status = _xioopen_foxec_end(xioflags, &fd->stream, groups, &opts);
   if (status < 0)  return status;
   if (status == 0) {	/* child */
      int numleft;

      if (setopt_path(opts, &path) < 0) {
	 /* this could be dangerous, so let us abort this child... */
	 Exit(1);
      }

      if ((numleft = leftopts(opts)) > 0) {
	 Error1("%d option(s) could not be used", numleft);
	 showleft(opts);
	 return STAT_NORETRY;
      }

      Info1("executing shell command \"%s\"", string);
      result = System(string);
      if (result != 0) {
	 Warn2("system(\"%s\") returned with status %d", string, result);
      }
      Exit(0);	/* this child process */
   }

   /* parent */
   return 0;
}

#endif /* WITH_SYSTEM */
