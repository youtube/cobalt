/*	$OpenBSD: err.c,v 1.2 2002/06/25 15:50:15 mickey Exp $	*/

/*
 * log.c
 *
 * Based on err.c, which was adapted from OpenBSD libc *err* *warn* code.
 *
 * Copyright (c) 2005 Nick Mathewson <nickm@freehaven.net>
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef STARBOARD
#if defined LIBEVENT_PLATFORM_HEADER
#include LIBEVENT_PLATFORM_HEADER
#else  //  defined LIBEVENT_PLATFORM_HEADER
#include "libevent-starboard.h"
#endif  //  defined LIBEVENT_PLATFORM_HEADER

#include "starboard/common/log.h"
#include "starboard/system.h"
#include "starboard/types.h"

// Include Starboard poems after all system headers.
#include "starboard/client_porting/poem/string_poem.h"
#else  // STARBOARD
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <sys/_libevent_time.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#endif  // STARBOARD

#include "event.h"
#include "log.h"
#include "evutil.h"

static void _warn_helper(int severity, int log_errno, const char *fmt,
                         va_list ap);
static void event_log(int severity, const char *msg);

void
event_err(int eval, const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	_warn_helper(_EVENT_LOG_ERR, errno, fmt, ap);
	va_end(ap);
#ifdef STARBOARD
  SbSystemBreakIntoDebugger();
#else
	exit(eval);
#endif
}

void
event_warn(const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	_warn_helper(_EVENT_LOG_WARN, errno, fmt, ap);
	va_end(ap);
}

void
event_errx(int eval, const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	_warn_helper(_EVENT_LOG_ERR, -1, fmt, ap);
	va_end(ap);
#ifdef STARBOARD
  SbSystemBreakIntoDebugger();
#else
	exit(eval);
#endif
}

void
event_warnx(const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	_warn_helper(_EVENT_LOG_WARN, -1, fmt, ap);
	va_end(ap);
}

void
event_msgx(const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	_warn_helper(_EVENT_LOG_MSG, -1, fmt, ap);
	va_end(ap);
}

void
_event_debugx(const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	_warn_helper(_EVENT_LOG_DEBUG, -1, fmt, ap);
	va_end(ap);
}

static void
_warn_helper(int severity, int log_errno, const char *fmt, va_list ap)
{
	char buf[1024];
	size_t len;

	if (fmt != NULL)
		evutil_vsnprintf(buf, sizeof(buf), fmt, ap);
	else
		buf[0] = '\0';

#ifndef STARBOARD
	if (log_errno >= 0) {
		len = strlen(buf);
		if (len < sizeof(buf) - 3) {
			evutil_snprintf(buf + len, sizeof(buf) - len, ": %s",
			    strerror(log_errno));
		}
	}
#endif

	event_log(severity, buf);
}

static event_log_cb log_fn = NULL;

void
event_set_log_callback(event_log_cb cb)
{
	log_fn = cb;
}

static void
event_log(int severity, const char *msg)
{
#ifdef STARBOARD
  SbLogPriority log_priority;
  switch (severity) {
    case _EVENT_LOG_DEBUG:
		case _EVENT_LOG_MSG:
      log_priority = kSbLogPriorityInfo;
      break;
    case _EVENT_LOG_WARN:
      log_priority = kSbLogPriorityWarning;
      break;
    case _EVENT_LOG_ERR:
      log_priority = kSbLogPriorityError;
      break;
    default:
      SB_NOTREACHED();
      log_priority = kSbLogPriorityUnknown;
      break;
  }
  SbLog(log_priority, msg);
#else  // STARBOARD
	if (log_fn)
		log_fn(severity, msg);
	else {
		const char *severity_str;
		switch (severity) {
		case _EVENT_LOG_DEBUG:
			severity_str = "debug";
			break;
		case _EVENT_LOG_MSG:
			severity_str = "msg";
			break;
		case _EVENT_LOG_WARN:
			severity_str = "warn";
			break;
		case _EVENT_LOG_ERR:
			severity_str = "err";
			break;
		default:
			severity_str = "???";
			break;
		}
		(void)fprintf(stderr, "[%s] %s\n", severity_str, msg);
	}
#endif  // STARBOARD
}
