/* $Id$ */
/* Copyright Gerhard Rieger 2006 */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef __xiosigchld_h
#define __xiosigchld_h 1

extern pid_t diedunknown1;	/* child died before it is registered */
extern pid_t diedunknown2;
extern pid_t diedunknown3;
extern pid_t diedunknown4;

extern int xiosetsigchild(xiofile_t *xfd, int (*callback)(struct single *));
extern void childdied(int signum
#if HAVE_SIGACTION
		      , siginfo_t *siginfo, void *context
#endif /* HAVE_SIGACTION */
		      );

extern int
   xiosigchld_register(pid_t pid,
			    void (*sigaction)(int, siginfo_t *, void *),
			    void *context);
extern int xiosigchld_unregister(pid_t pid);
extern int xiosigchld_clearall(void);

extern void xiosigaction_subaddr_ok(int signum, siginfo_t *siginfo, void *ucontext);
extern void xiosigaction_child(int signum, siginfo_t *siginfo, void *ucontext);

#endif /* !defined(__xiosigchld_h) */
