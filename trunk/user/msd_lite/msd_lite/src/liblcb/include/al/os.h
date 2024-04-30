/*-
 * Copyright (c) 2011-2023 Rozhuk Ivan <rozhuk.im@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Rozhuk Ivan <rozhuk.im@gmail.com>
 *
 */


#ifndef __ABSTRACTION_LAYER_OS_H__
#define __ABSTRACTION_LAYER_OS_H__

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h> /* open, fcntl */
#include <string.h> /* bcopy, bzero, memcpy, memmove, memset, strnlen, strerror... */
#include <unistd.h> /* close, write, sysconf */
#include <pthread.h>

/* Secure version of memset(). */
static void *(*volatile memset_volatile)(void*, int, size_t) = memset;


/* Compiler things. */
#ifndef __unused
#	define __unused		__attribute__((__unused__))
#endif

#ifndef offsetof /* offsetof struct field */
#	define offsetof(__type, __field)				\
		((size_t)((const volatile void*)&((__type*)0)->__field))
#endif


/* Constants. */
#ifndef SIZE_T_MAX
#	define SIZE_T_MAX	((size_t)~0)
#endif

#ifndef CLOCK_REALTIME_FAST
#	define CLOCK_REALTIME_FAST	CLOCK_REALTIME
#endif

#ifndef CLOCK_MONOTONIC_FAST
#	define CLOCK_MONOTONIC_FAST	CLOCK_MONOTONIC
#endif

#if !defined(TCP_KEEPIDLE) && defined(TCP_KEEPALIVE)
#	define TCP_KEEPIDLE	TCP_KEEPALIVE
#endif

#if !defined(NOTE_ABSTIME) && defined(NOTE_ABSOLUTE)
#	define NOTE_ABSTIME	NOTE_ABSOLUTE
#endif

#ifndef NOTE_MSECONDS
#	define NOTE_MSECONDS	0
#endif



/* Struct field access. */
#ifndef s6_addr32
#	define s6_addr32	__u6_addr.__u6_addr32
#endif

#ifndef ifr_ifindex
#	define ifr_ifindex	ifr_ifru.ifru_index
#endif

#ifndef _D_EXACT_NAMLEN /* From linux: dirent.h */
#	ifdef _DIRENT_HAVE_D_NAMLEN
#		define _D_EXACT_NAMLEN(__de)	((__de)->d_namlen)
#	else
#		define _D_EXACT_NAMLEN(__de)	(strnlen((__de)->d_name, sizeof((__de)->d_name)))
#	endif
#endif /* _D_EXACT_NAMLEN */

#ifndef TAILQ_PREV_PTR
#	define TAILQ_PREV_PTR(elm, field)	((elm)->field.tqe_prev)
#endif


/* Macros for counting and rounding. */
#ifndef nitems /* SIZEOF() */
#	define nitems(__val)	(sizeof(__val) / sizeof(__val[0]))
#endif

#ifndef howmany
#	define	howmany(__x, __y) (((__x) + ((__y) - 1)) / (__y))
#endif

#ifndef rounddown
#	define rounddown(__x, __y) (((__x) / (__y)) * (__y)) /* To any y. */
#endif

#ifndef rounddown2
#	define rounddown2(__x, __y) ((__x) & (~((__y) - 1))) /* If y is power of two. */
#endif

#ifndef roundup
#	define roundup(__x, __y) ((((__x) + ((__y) - 1)) / (__y)) * (__y)) /* To any y. */
#endif

#ifndef roundup2
#	define roundup2(__x, __y) (((__x) + ((__y) - 1)) & (~((__y) - 1))) /* If y is power of two. */
#endif

#ifndef powerof2
#	define powerof2(__x)	((((__x) - 1) & (__x)) == 0)
#endif

/* Macro functions. */
#ifndef MIN
#	define MIN(__a, __b)	(((__a) < (__b)) ? (__a) : (__b))
#endif

#ifndef MAX
#	define MAX(__a, __b)	(((__a) > (__b)) ? (__a) : (__b))
#endif

#ifndef IN_LOOPBACK
#	define IN_LOOPBACK(__x) (0x7f000000 == (0xff000000 & (uint32_t)(__x)))
#endif

#ifndef IN_BROADCAST
#	define IN_BROADCAST(__x) (INADDR_BROADCAST == (uint32_t)(__x))
#endif

#ifndef IN_MULTICAST
#	define IN_MULTICAST(__x) (0xe0000000 == (0xf0000000 & (uint32_t)(__x)))
#endif

#ifndef IN6_IS_ADDR_MULTICAST
#	define IN6_IS_ADDR_MULTICAST(__x) (0xff == (__x)->s6_addr[0])
#endif

#ifndef TAILQ_FOREACH_SAFE
#	define TAILQ_FOREACH_SAFE(__var, __head, __field, __tvar)	\
		for ((__var) = TAILQ_FIRST((__head));			\
		    (__var) && ((__tvar) = TAILQ_NEXT((__var), __field), 1); \
		    (__var) = (__tvar))
#endif

#ifndef TAILQ_SWAP
#	define TAILQ_SWAP(__head1, __head2, __type, __field) do {	\
		struct __type *swap_first = (__head1)->tqh_first;	\
		struct __type **swap_last = (__head1)->tqh_last;	\
		(__head1)->tqh_first = (__head2)->tqh_first;		\
		(__head1)->tqh_last = (__head2)->tqh_last;		\
		(__head2)->tqh_first = swap_first;			\
		(__head2)->tqh_last = swap_last;			\
		if (NULL != (swap_first = (__head1)->tqh_first)) {	\
			swap_first->__field.tqe_prev = &(__head1)->tqh_first; \
		} else {						\
			(__head1)->tqh_last = &(__head1)->tqh_first;	\
		}							\
		if (NULL != (swap_first = (__head2)->tqh_first)) {	\
			swap_first->__field.tqe_prev = &(__head2)->tqh_first; \
		} else {						\
			(__head2)->tqh_last = &(__head2)->tqh_first;	\
		}							\
	} while (0)
#endif


/* Functions. */

#ifndef HAVE_EXPLICIT_BZERO
static inline void
explicit_bzero(void *b, size_t len) {

	if (NULL == b || 0 == len)
		return;
	memset_volatile(b, 0x00, len);
}
#endif

#ifndef HAVE_MEMRCHR
static inline void *
memrchr(const void *buf, const int what_find, const size_t buf_size) {
	register uint8_t *ptm;

	if (NULL == buf || 0 == buf_size)
		return (NULL);

	ptm = (((uint8_t*)buf) + buf_size - 1);
	while (ptm >= (uint8_t*)buf) {
		if ((*ptm) == (uint8_t)what_find)
			return (ptm);
		ptm --;
	}
	return (NULL);
}
#endif

#ifndef HAVE_MEMMEM
static inline void *
memmem(const void *buf, const size_t buf_size, const void *what_find,
    const size_t what_find_size) {
	register uint8_t *ptm;
	register size_t buf_size_wrk;

	if (NULL == buf || 0 == what_find_size || what_find_size > buf_size)
		return (NULL);
	if (1 == what_find_size) /* Use fast memchr(). */
		return ((void*)memchr(buf, (*((uint8_t*)what_find)), buf_size));
	if (what_find_size == buf_size) { /* Only memcmp(). */
		if (0 == memcmp(buf, what_find, what_find_size))
			return ((void*)buf);
		return (NULL);
	}

	ptm = ((uint8_t*)buf);
	buf_size_wrk = (buf_size - (what_find_size - 1));
	for (;;) {
		ptm = (uint8_t*)memchr(ptm, (*((uint8_t*)what_find)),
		    (buf_size_wrk - (ptm - ((uint8_t*)buf))));
		if (NULL == ptm)
			return (NULL);
		if (0 == memcmp(ptm, what_find, what_find_size))
			return (ptm);
		ptm ++;
	}
	return (NULL);
}
#endif


#ifndef HAVE_REALLOCARRAY
static inline void *
reallocarray(void *ptr, const size_t nmemb, const size_t size) {
	size_t nmemb_size;

	nmemb_size = (nmemb * size);
	if (0 == nmemb_size) {
		if (0 != nmemb &&
		    0 != size) { /* Overflow. */
			errno = ENOMEM;
			return (NULL);
		}
		nmemb_size ++;
	} else if (((nmemb | size) & (SIZE_T_MAX << (sizeof(size_t) * 4))) &&
	    (nmemb_size / size) != nmemb) { /* size_t overflow. */
		errno = ENOMEM;
		return (NULL);
	}

	return (realloc(ptr, nmemb_size));
}
#endif

#ifndef HAVE_FREEZERO
static inline void
freezero(void *ptr, const size_t size) {

	if (NULL == ptr)
		return;
	memset_volatile(ptr, 0x00, size);
	free(ptr);
}
#endif


#ifndef HAVE_STRLCPY
static inline size_t
strlcpy(char * restrict dst, const char * restrict src, size_t size) {
	size_t src_size, cp_size;

	if (NULL == dst || NULL == src || 0 == size)
		return (0);

	src_size = strlen(src);
	cp_size = ((src_size < size) ? src_size : (size - 1));
	memcpy(dst, src, cp_size);
	dst[cp_size] = 0x00;

	return (src_size);
}
#endif


/* Syscalls. */

/* pthread_create(2) can spuriously fail on Linux. This is a function
 * to wrap pthread_create(2) to retry if it fails with EAGAIN. */
static inline int
pthread_create_eagain(pthread_t *handle, const pthread_attr_t *attr,
    void *(*fn)(void*), void *arg) {
	int error;
	const size_t max_tries = 20;
	struct timespec rqts = { .tv_sec = 0 };

	for (size_t i = 1; i <= max_tries; i ++) {
		error = pthread_create(handle, attr, fn, arg);
		if (0 == error || /* Ok, done. */
		    EAGAIN != error) /* Other error. */
			return (error);
		/* Retry after tries * 1 millisecond. */
		rqts.tv_nsec = (long)(i * 1000 * 1000);
		nanosleep(&rqts, NULL); /* Ignore early wakeup and errors. */
	}

	return (EAGAIN);
}


#ifndef HAVE_PIPE2
static inline int
pipe2(int fildes[2], int flags) {
	int error;

	error = pipe(fildes);
	if (0 != error)
		return (error);
	if (0 != (O_NONBLOCK & flags)) {
		if (-1 == fcntl((int)fildes[0], F_SETFL, O_NONBLOCK) ||
		    -1 == fcntl((int)fildes[1], F_SETFL, O_NONBLOCK)) {
			error = errno;
			goto err_out;
		}
	}

	return (0);

err_out:
	close(fildes[0]);
	close(fildes[1]);
	fildes[0] = -1;
	fildes[1] = -1;

	return (error);
}
#endif

#ifndef HAVE_ACCEPT4
/* On systems without SOCK_NONBLOCK and accept4() we must define
 * SOCK_NONBLOCK to compile and use accept4().
 * Use HAVE_SOCK_NONBLOCK to make sure that you will not pass
 * SOCK_NONBLOCK to any other function in your code. */
#ifndef SOCK_NONBLOCK
#	define SOCK_NONBLOCK	0x20000000
#endif
static inline int
accept4(int skt, struct sockaddr *addr, socklen_t *addrlen, int flags) {
	int s;

	s = accept(skt, addr, addrlen);
	if (-1 == s)
		return (-1);
	if (0 != (SOCK_NONBLOCK & flags)) {
		if (-1 == fcntl((int)s, F_SETFL, O_NONBLOCK)) {
			close(s);
			return (-1);
		}
	}

	return (s);
}
#endif


#endif /* __ABSTRACTION_LAYER_OS_H__ */
