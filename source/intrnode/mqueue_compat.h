/*
 * mqueue_compat.h - POSIX message queue compatibility for macOS
 *
 * macOS does not provide <mqueue.h> (POSIX message queues).  This header
 * emulates the subset of the mqueue API used by Tournament Trivia, using
 * UNIX-domain datagram sockets.  Datagram sockets preserve message boundaries
 * and support non-blocking I/O, giving semantics close to mqueue.
 *
 * Supported functions:
 *   mq_open, mq_close, mq_unlink, mq_send, mq_receive, mq_getattr
 */

#ifndef MQUEUE_COMPAT_H
#define MQUEUE_COMPAT_H

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef int mqd_t;

struct mq_attr {
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
};

/* Convert an mqueue name (e.g. "/trvinput") to a socket path under /tmp. */
static inline void _mq_name_to_path(const char *name, char *path, size_t pathlen)
{
    const char *clean = (name[0] == '/') ? name + 1 : name;
    snprintf(path, pathlen, "/tmp/.mq_%s", clean);
}

/*
 * mq_open - create or open a message queue
 *
 * Supports the two calling conventions used by the project:
 *   mq_open(name, O_CREAT | O_EXCL | O_RDONLY | O_NONBLOCK, mode, &attr)
 *   mq_open(name, O_WRONLY | O_NONBLOCK)
 */
static inline mqd_t mq_open(const char *name, int oflag, ...)
{
    char path[256];
    _mq_name_to_path(name, path, sizeof(path));

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
        return (mqd_t)-1;

    if (oflag & O_CREAT)
    {
        /* Reader side: bind a datagram socket to the path. */
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            if (errno == EADDRINUSE && (oflag & O_EXCL))
            {
                /* Mimic mqueue EEXIST when O_EXCL is set. */
                close(fd);
                errno = EEXIST;
                return (mqd_t)-1;
            }
            /* Unexpected bind error. */
            close(fd);
            return (mqd_t)-1;
        }

        if (oflag & O_NONBLOCK)
        {
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }
    else
    {
        /* Writer side: connect to the receiver's socket. */
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            close(fd);
            return (mqd_t)-1;
        }

        if (oflag & O_NONBLOCK)
        {
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    return fd;
}

static inline int mq_close(mqd_t mqdes)
{
    return close(mqdes);
}

static inline int mq_unlink(const char *name)
{
    char path[256];
    _mq_name_to_path(name, path, sizeof(path));
    return unlink(path);
}

static inline int mq_send(mqd_t mqdes, const char *msg_ptr,
                           size_t msg_len, unsigned int msg_prio)
{
    (void)msg_prio;
    ssize_t ret = send(mqdes, msg_ptr, msg_len, 0);
    return (ret >= 0) ? 0 : -1;
}

static inline ssize_t mq_receive(mqd_t mqdes, char *msg_ptr,
                                  size_t msg_len, unsigned int *msg_prio)
{
    if (msg_prio)
        *msg_prio = 0;
    return recv(mqdes, msg_ptr, msg_len, 0);
}

static inline int mq_getattr(mqd_t mqdes, struct mq_attr *attr)
{
    if (!attr)
    {
        errno = EINVAL;
        return -1;
    }

    memset(attr, 0, sizeof(*attr));
    attr->mq_msgsize = 256; /* matches MQ_MAX_MSG_SIZE */

    /*
     * For datagram sockets, FIONREAD returns the size of the first
     * pending datagram.  If > 0 there is at least one message queued.
     */
    int bytes_available = 0;
    if (ioctl(mqdes, FIONREAD, &bytes_available) < 0)
        return -1;

    attr->mq_curmsgs = (bytes_available > 0) ? 1 : 0;
    return 0;
}

#endif /* MQUEUE_COMPAT_H */
