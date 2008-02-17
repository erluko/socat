/* $Id: xiosigchld.c,v 1.21 2006/12/28 14:38:38 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2007 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this is the source of the extended child signal handler */


#include "xiosysincludes.h"
#include "xioopen.h"
#include "xiosigchld.h"

/*****************************************************************************/
/* we maintain a table of all known child processes */
/* for now, we use a very primitive data structure - just an unsorted, size
   limited array */

#define XIO_MAXCHILDPIDS 16

struct _xiosigchld_child {
   pid_t pid;
   void (*sigaction)(int, siginfo_t *, void *);
   void *context;
} ;

static struct _xiosigchld_child * _xiosigchld_find(pid_t pid);

static struct _xiosigchld_child xio_childpids[XIO_MAXCHILDPIDS];

#if 1 /*!!!*/
/*!! with socat, at most 4 managed children exist */
pid_t diedunknown1;	/* child died before it is registered */
pid_t diedunknown2;
pid_t diedunknown3;
pid_t diedunknown4;
#endif


/* register for a xio filedescriptor a callback (handler).
   when a SIGCHLD occurs, the signal handler will ??? */
int xiosetsigchild(xiofile_t *xfd, int (*callback)(struct single *)) {
   if (xfd->tag != XIO_TAG_DUAL) {
      xfd->stream.child.sigchild = callback;
   } else {
      xfd->dual.stream[0]->child.sigchild = callback;
      xfd->dual.stream[1]->child.sigchild = callback;
   }
   return 0;
}

/* exec'd child has died, perform appropriate changes to descriptor */
static int sigchld_stream(struct single *file) {
   /*!! call back to application */
   file->child.pid = 0;
   if (file->child.sigchild) {
      return (*file->child.sigchild)(file);
   }
   return 0;
}

/* return 0 if socket is not responsible for deadchild */
static int xio_checkchild(xiofile_t *socket, int socknum, pid_t deadchild) {
   int retval;
   if (socket != NULL) {
      if (socket->tag != XIO_TAG_DUAL) {
	 if (socket->stream.child.pid == deadchild) {
	    Info2("exec'd process %d on socket %d terminated",
		  socket->stream.child.pid, socknum);
	    sigchld_stream(&socket->stream);
	    return 1;
	 }
      } else {
	 if (retval = xio_checkchild((xiofile_t *)socket->dual.stream[0], socknum, deadchild))
	    return retval;
	 else
	    return xio_checkchild((xiofile_t *)socket->dual.stream[1], socknum, deadchild);
      }
   }
   return 0;
}

/* this is the "physical" signal handler for SIGCHLD */
/* the current socat/xio implementation knows two kinds of children:
   exec/system addresses perform a fork: their children are registered and
   their death's influence the parents' flow;
   listen-socket with fork children: these children are "anonymous" and their
   death does not affect the parent process (now; maybe we have a child
   process counter later) */
void childdied(int signum
#if HAVE_SIGACTION
	       , siginfo_t *siginfo, void *context
#endif /* HAVE_SIGACTION */
	       ) {
   pid_t pid;
   int _errno;
   int status = 0;
   bool wassig = false;
   int i;
   struct _xiosigchld_child *entry;

   _errno = errno;	/* save current value; e.g., select() on Cygwin seems
			   to set it to EINTR _before_ handling the signal, and
			   then passes the value left by the signal handler to
			   the caller of select(), accept() etc. */
   /* is not thread/signal save, but confused messages in rare cases are better
      than no messages at all */
   Info1("childdied(signum=%d)", signum);
   do {
      pid = Waitpid(-1, &status, WNOHANG);
      if (pid == 0) {
	 Msg(wassig?E_INFO:E_WARN,
	     "waitpid(-1, {}, WNOHANG): no child has exited");
	 Info("childdied() finished");
	 errno = _errno;
	 return;
      } else if (pid < 0 && errno == ECHILD) {
	 Msg1(wassig?E_INFO:E_WARN,
	      "waitpid(-1, {}, WNOHANG): %s", strerror(errno));
	 Info("childdied() finished");
	 errno = _errno;
	 return;
      }
      wassig = true;
      if (pid < 0) {
	 Warn2("waitpid(-1, {%d}, WNOHANG): %s", status, strerror(errno));
	 Info("childdied() finished");
	 errno = _errno;
	 return;
      }
#if 0
   /*! indent */
   /* check if it was a registered child process */
   i = 0;
   while (i < XIO_MAXSOCK) {
      if (xio_checkchild(sock[i], i, pid))  break;
      ++i;
   }
   if (i == XIO_MAXSOCK) {
	 Info2("childdied(%d): cannot identify child %d", signum, pid);
	 if (diedunknown1 == 0) {
	    diedunknown1 = pid;
	    Debug("saving pid in diedunknown1");
	 } else if (diedunknown2 == 0) {
	    diedunknown2 = pid;
	    Debug("saving pid in diedunknown2");
	 } else if (diedunknown3 == 0) {
	     diedunknown3 = pid;
	    Debug("saving pid in diedunknown3");
	 } else {
	    diedunknown4 = pid;
	    Debug("saving pid in diedunknown4");
	 }
      }
#else
   entry = _xiosigchld_find(pid);
   if (entry == NULL) {
      Info("dead child "F_pid" died unknown");
   } else {
      (*entry->sigaction)(signum, siginfo, entry->context);
      xiosigchld_unregister(pid);
   }
#endif

   if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == 0) {
	 Info2("waitpid(): child %d exited with status %d",
	       pid, WEXITSTATUS(status));
      } else {
	 Warn2("waitpid(): child %d exited with status %d",
	       pid, WEXITSTATUS(status));
      }
   } else if (WIFSIGNALED(status)) {
      Info2("waitpid(): child %d exited on signal %d",
	    pid, WTERMSIG(status));
   } else if (WIFSTOPPED(status)) {
      Info2("waitpid(): child %d stopped on signal %d",
	    pid, WSTOPSIG(status));
   } else {
      Warn1("waitpid(): cannot determine status of child %d", pid);
   }

#if !HAVE_SIGACTION
   /* we might need to re-register our handler */
   if (Signal(SIGCHLD, childdied) == SIG_ERR) {
      Warn2("signal(SIGCHLD, %p): %s", childdied, strerror(errno));
   }
#endif /* !HAVE_SIGACTION */
  } while (1);
   Info("childdied() finished");
   errno = _errno;
}


/* search for given pid in child process table. returns matching entry on
success, or NULL if not found. Can be used with pid==0 to look for an empty
entry. */
static struct _xiosigchld_child *
   _xiosigchld_find(pid_t pid) {
   
   int i;

   /* is it already registered? */
   for (i = 0; i < XIO_MAXCHILDPIDS; ++i) {
      if (pid == xio_childpids[i].pid) {
	 return &xio_childpids[i];
      }
   }
   return NULL;
}

/* add a child process to the table
   returns 0 on success (registered or reregistered child)
   returns -1 on table overflow
 */
int xiosigchld_register(pid_t pid,
			     void (*sigaction)(int, siginfo_t *, void *),
			     void *context) {
   struct _xiosigchld_child *entry;

   /* is it already registered? */
   if (entry = _xiosigchld_find(pid)) {
      /* was already registered, override */
      entry->sigaction = sigaction;
      entry->context = context;
      return 0;
   }

   /* try to register it */
   if (entry = _xiosigchld_find(0)) {
      entry->pid = pid;
      entry->sigaction = sigaction;
      entry->context = context;
      return 0;
   }
   Warn("xiosigchld_register(): table overflow");
   return -1;
}

/* remove a child process to the table
   returns 0 on success
   returns 1 if pid was not found in table
 */
int xiosigchld_unregister(pid_t pid) {
   struct _xiosigchld_child *entry;

   /* is it already registered? */
   if (entry = _xiosigchld_find(pid)) {
      /* found, remove it from table */
      entry->pid = 0;
      return 0;
   }
   return 1;
}

/* clear the child process table */
/* especially interesting after fork() in child process
   returns 0
 */
int xiosigchld_clearall(void) {
   int i;

   for (i = 0; i < XIO_MAXCHILDPIDS; ++i) {
      xio_childpids[i].pid = 0;
   }
   return 0;
}


void xiosigaction_subaddr_ok(int signum, siginfo_t *siginfo, void *ucontext) {
   pid_t subpid = siginfo->si_pid;
   struct _xiosigchld_child *entry;
   xiosingle_t *xfd;

   entry = _xiosigchld_find(subpid);
   if (entry == NULL) {
      Warn1("SIGUSR1 from unregistered process "F_pid, subpid);
      return;
   }
   xfd = entry->context;
   xfd->subaddrstat = 1;
}

void xiosigaction_child(int signum, siginfo_t *siginfo, void *ucontext) {
   pid_t subpid = siginfo->si_pid;
   xiosingle_t *xfd = ucontext;

   /* the sub process that is connected to this xio address has terminated */
   Notice2("sub process "F_pid" died, setting in xfd %p", subpid, xfd);
   xfd->subaddrstat = -1;
   xfd->subaddrexit = siginfo->si_status;
   if (xfd->child.sigchild) {
      (*xfd->child.sigchild)(xfd);
   }
}

