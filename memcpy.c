/*****************************************************************************

  Copyright (c) 2015 Andre McCurdy <armccurdy@gmail.com>

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
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>

static void * (*memcpy_original) (void *dest, const void *src, size_t n);
static int64_t n_total, delta_usec_total;
static size_t too_small_to_measure = 512;
static int logfd = -1;

static double mbps (int64_t n, int64_t usec)
{
	double mbytes = ((double) n) / (1024.0 * 1024.0);
	double seconds = ((double) usec) / 1000000.0;

	return mbytes / seconds;
}

void *memcpy (void *dest, const void *src, size_t n)
{
	char buf[160];
	struct timeval start, finish;
	int64_t delta_usec;
	ssize_t write_bytes;
	char *logfile;
	int len;
	int fd;

	/*
	   The outcome of casting (void *) to a function pointer is undefined,
	   so to avoid a potential compiler warning, cast the address of the
	   function pointer to a pointer to (void *) instead. See the dlsym
	   manpage.
	*/
	if (!memcpy_original)
		*(void **) (&memcpy_original) = dlsym (RTLD_NEXT, __FUNCTION__);

	if (n < (2 * too_small_to_measure))
		return memcpy_original (dest, src, n);

	gettimeofday (&start, NULL);
	memcpy_original (dest, src, n);
	gettimeofday (&finish, NULL);

	delta_usec = finish.tv_usec - start.tv_usec;
	while (finish.tv_sec > start.tv_sec) {
		delta_usec += 1000000;
		start.tv_sec++;
	}

	if (delta_usec <= 1) {
		too_small_to_measure = n;
		return dest;
	}

	n_total += n;
	delta_usec_total += delta_usec;

	len = sprintf (buf, "memcpy (0x%0*" PRIxPTR ", 0x%0*" PRIxPTR ", %7llu) : %5llu us, %5.0f MB/s : %6llu MB @ %5.0f MB/s\n",
	               (int) sizeof(uintptr_t) * 2, (uintptr_t) dest,
	               (int) sizeof(uintptr_t) * 2, (uintptr_t) src,
	               (unsigned long long) n,
	               (unsigned long long) delta_usec, mbps (n, delta_usec),
	               (unsigned long long) n_total / (1024 * 1024), mbps (n_total, delta_usec_total));

	/*
	   If we've never tried to open a log file, then create one based on
	   the INTERPOSE_MEMCPY_LOGFILE environment variable.
	   If opening the log file fails for any reason, then send logging data
	   to a bad fd, so the information can still be seen by using strace.
	*/
	if (logfd == -1) {
		logfile = getenv ("INTERPOSE_MEMCPY_LOGFILE");
		if (logfile && ((fd = open (logfile, (O_WRONLY | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) >= 0))
			logfd = fd;
		else
			logfd = 99999;
	}

	write_bytes = write (logfd, buf, len);

	(void) write_bytes;

	return dest;
}
