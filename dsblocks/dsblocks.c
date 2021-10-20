#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "shared.h"

#define LOCKFILE                        "/var/local/dsblocks/dsblocks.pid"

#define DELIMITERLENGTH                 (sizeof delimiter)
#define STATUSLENGTH                    (LENGTH(blocks) * (BLOCKLENGTH + DELIMITERLENGTH) + 1)

#include "config.h"

_Static_assert(INTERVALs >= 0, "INTERVALs must be greater than or equal to 0");
_Static_assert(INTERVALn >= 0 && INTERVALn <= 999999999, "INTERVALn must be between 0 and 999999999");

void cleanup(void);

static void buttonhandler(int sig, siginfo_t *info, void *ucontext);
static void setupsignals(void);
static void sighandler(int sig, siginfo_t *info, void *ucontext);
static void statusloop(void);
static void termhandler(int sig);
static void updateblock(Block *block, int sigval);
static void updatestatus(void);
static void writepid(void);

pid_t pid;

static Block *dirtyblock;
static Display *dpy;
static sigset_t blocksigmask;

void
buttonhandler(int sig, siginfo_t *info, void *ucontext)
{
        sig = info->si_value.sival_int >> 8;
        for (Block *block = blocks; block->funcu; block++)
                if (block->signal == sig)
                        switch (fork()) {
                                case -1:
                                        perror("buttonhandler - fork");
                                        break;
                                case 0:
                                        block->funcc(info->si_value.sival_int & 0xff);
                                        exit(0);
                        }
}

void
cleanup(void)
{
        unlink(LOCKFILE);
        XStoreName(dpy, DefaultRootWindow(dpy), "");
        XCloseDisplay(dpy);
}

void
setupsignals(void)
{
        struct sigaction sa;

        /* populate blocksigmask and check validity of signals */
        sigemptyset(&blocksigmask);
        sigaddset(&blocksigmask, SIGHUP);
        sigaddset(&blocksigmask, SIGINT);
        sigaddset(&blocksigmask, SIGTERM);
        for (Block *block = blocks; block->funcu; block++) {
                if (block->signal <= 0)
                        continue;
                if (block->signal > SIGRTMAX - SIGRTMIN) {
                        fprintf(stderr, "Error: SIGRTMIN + %d exceeds SIGRTMAX.\n", block->signal);
                        unlink(LOCKFILE);
                        XCloseDisplay(dpy);
                        exit(2);
                }
                sigaddset(&blocksigmask, SIGRTMIN + block->signal);
        }

        /* setup signal handlers */
        /* to handle HUP, INT and TERM */
        sa.sa_flags = SA_RESTART;
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = termhandler;
        sigaction(SIGHUP, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        /* to ignore unused realtime signals */
        // sa.sa_flags = SA_RESTART;
        // sigemptyset(&sa.sa_mask);
        sa.sa_handler = SIG_IGN;
        for (int i = SIGRTMIN + 1; i <= SIGRTMAX; i++)
                sigaction(i, &sa, NULL);

        /* to prevent forked children from becoming zombies */
        sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
        // sigemptyset(&sa.sa_mask);
        sa.sa_handler = SIG_IGN;
        sigaction(SIGCHLD, &sa, NULL);

        /* to handle signals generated by dwm on click events */
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        // sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = buttonhandler;
        sigaction(SIGRTMIN, &sa, NULL);

        /* to handle update signals for individual blocks */
        sa.sa_flags = SA_NODEFER | SA_RESTART | SA_SIGINFO;
        sa.sa_mask = blocksigmask;
        sa.sa_sigaction = sighandler;
        for (Block *block = blocks; block->funcu; block++)
                if (block->signal > 0)
                        sigaction(SIGRTMIN + block->signal, &sa, NULL);
}

void
sighandler(int sig, siginfo_t *info, void *ucontext)
{
        sig -= SIGRTMIN;
        for (Block *block = blocks; block->funcu; block++)
                if (block->signal == sig)
                        updateblock(block, info->si_value.sival_int);
        updatestatus();
}

void
statusloop(void)
{
        int i;
        struct timespec t;

        sigprocmask(SIG_BLOCK, &blocksigmask, NULL);
        for (Block *block = blocks; block->funcu; block++)
                if (block->interval >= 0)
                        updateblock(block, NILL);
        for (i = 1; ; i++) {
                updatestatus();
                sigprocmask(SIG_UNBLOCK, &blocksigmask, NULL);
                t.tv_sec = INTERVALs, t.tv_nsec = INTERVALn;
                while (nanosleep(&t, &t) == -1);
                sigprocmask(SIG_BLOCK, &blocksigmask, NULL);
                for (Block *block = blocks; block->funcu; block++)
                        if (block->interval > 0 && i % block->interval == 0)
                                updateblock(block, NILL);
        }
}

void
termhandler(int sig)
{
        cleanup();
        exit(0);
}

void
updateblock(Block *block, int sigval)
{
        size_t l;

        if (!(l = block->funcu(block->curtext, sigval)))
                return;
        if (memcmp(block->curtext, block->prvtext, l) != 0) {
                memcpy(block->prvtext, block->curtext, l);
                if (!dirtyblock || block < dirtyblock)
                        dirtyblock = block;
        }
        if (l == 1)
                block->length = 0;
        else {
                if (block->funcc)
                        block->curtext[l - 1] = block->signal;
                else
                        l--;
                memcpy(block->curtext + l, delimiter, DELIMITERLENGTH);
                block->length = l + DELIMITERLENGTH;
        }
}

void
updatestatus(void)
{
        static char statustext[STATUSLENGTH];
        char *s = statustext;
        Block *block;

        if (!dirtyblock)
                return;
        for (block = blocks; block < dirtyblock; block++)
                s += block->length;
        for (; block->funcu; block++) {
                memcpy(s, block->curtext, block->length);
                s += block->length;
        }
        s[s == statustext ? 0 : -DELIMITERLENGTH] = '\0';
        dirtyblock = NULL;

        XStoreName(dpy, DefaultRootWindow(dpy), statustext);
        XSync(dpy, False);
}

void
writepid(void)
{
        int fd;
        struct flock fl;

        if ((fd = open(LOCKFILE, O_RDWR | O_CREAT | O_CLOEXEC,
                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
                perror("writepid - open");
                exit(1);
        }
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;
        if (fcntl(fd, F_SETLK, &fl) == -1) {
                if (errno == EACCES || errno == EAGAIN) {
                        close(fd);
                        fputs("Error: another instance of dsblocks is already running.\n", stderr);
                        exit(2);
                }
                perror("writepid - fcntl");
                close(fd);
                exit(1);
        }
        if (ftruncate(fd, 0) == -1) {
                perror("writepid - ftruncate");
                close(fd);
                exit(1);
        }
        if (dprintf(fd, "%ld", (long)pid) < 0) {
                perror("writepid - dprintf");
                close(fd);
                exit(1);
        }
}

int
main(int argc, char *argv[])
{
        int xfd;

        pid = getpid();
        writepid();
        if (!(dpy = XOpenDisplay(NULL))) {
                fputs("Error: could not open display.\n", stderr);
                unlink(LOCKFILE);
                return 1;
        }
        xfd = ConnectionNumber(dpy);
        fcntl(xfd, F_SETFD, fcntl(xfd, F_GETFD) | FD_CLOEXEC);
        setupsignals();
        statusloop();
        cleanup();
        return 0;
}