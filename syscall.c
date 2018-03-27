/*****************************************************************************

  Copyright (c) 2018 Andre McCurdy <armccurdy@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

*****************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/syscall.h>

#if defined (__x86_64__) && (SIZE_MAX == 0xFFFFFFFF)
#error "x32 not yet supported"
#endif

typedef long syscall_arg_t;

static int (*syscall_original) (int number, ...);
static int logfd = -1;

long int syscall (long int number, ...)
{
	char buf[256];
	va_list ap;
	syscall_arg_t a,b,c,d,e,f;
	int result;
	int errno_saved;
	ssize_t write_bytes;
	char *logfile;
	int fd;

	/*
	   The outcome of casting (void *) to a function pointer is undefined,
	   so to avoid a potential compiler warning, cast the address of the
	   function pointer to a pointer to (void *) instead. See the dlsym
	   manpage.
	*/
	if (!syscall_original)
		*(void **) (&syscall_original) = dlsym (RTLD_NEXT, __FUNCTION__);

	va_start (ap, number);
	a = va_arg (ap, syscall_arg_t);
	b = va_arg (ap, syscall_arg_t);
	c = va_arg (ap, syscall_arg_t);
	d = va_arg (ap, syscall_arg_t);
	e = va_arg (ap, syscall_arg_t);
	f = va_arg (ap, syscall_arg_t);
	va_end (ap);

	sprintf (buf, "syscall (%3ld, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)", number, a, b, c, d, e, f);

	/*
	   Default behaviour is to block SYS_renameat2, but for debug purposes,
	   let it through if the "ALLOW_RENAMEAT2" environment variable is set.
	*/
	if ((number == SYS_renameat2) && (getenv ("ALLOW_RENAMEAT2") == NULL)) {
		strcat (buf, " (* BLOCKED *)");
		result = -1;
		errno_saved = ENOSYS;
	}
	else {
		result = syscall_original (number, a, b, c, d, e, f);
		errno_saved = errno;
	}

	strcat (buf, "\n");

	/*
	   If we've never tried to open a log file, then create one based on
	   the INTERPOSE_SYSCALL_LOGFILE environment variable.
	   If opening the log file fails for any reason, then send logging data
	   to a bad fd, so the information can still be seen by using strace.
	*/
	if (logfd == -1) {
		logfile = getenv ("INTERPOSE_SYSCALL_LOGFILE");
		if (logfile && ((fd = open (logfile, (O_WRONLY | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) >= 0))
			logfd = fd;
		else
			logfd = 99999;
	}

	write_bytes = write (logfd, buf, strlen(buf));
	(void) write_bytes;

	errno = errno_saved;
	return result;
}
