/*
  memqueue
  (C) 2011  Hasan Alayli <halayli@gmail.com>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  log.c
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <err.h>
#include <fcntl.h>
#include <time.h>

#include <lthread.h>
#include "log.h"

struct _log {
    int fd;
    char *file_path;
    int level;
};

static char *log_levels[] = {
    [LOG_LEVEL_INFO] = "[INFO]: ",
    [LOG_LEVEL_WARN] = "[WARN]: ",
    [LOG_LEVEL_TRACE] = " [TRC]: ",
    [LOG_LEVEL_ERROR] = " [ERR]: ",
    [LOG_LEVEL_NOOP] = "[NOOP]: ",
};

char log_fmt[1024] = {0};

static struct _log l = {0};

void
log_(enum log_levels level, const char *module, const char *fmt, ...)
{
     va_list arglist;
    if (level != LOG_LEVEL_ERROR && l.level < level)
        return;

    time_t now;
    time(&now);
    if (level == LOG_LEVEL_ERROR)
        strcpy(log_fmt, "\033[31m");
    else
        strcpy(log_fmt, "\033[0m");

    strcpy((char *)(log_fmt + strlen(log_fmt)), ctime(&now));
    sprintf(log_fmt + strlen(log_fmt) - 5, " %-15s %s %s\n", module, log_levels[level], fmt);

    if (l.fd == -1) {
        va_start(arglist, fmt);
        vdprintf(2, log_fmt, arglist);
        va_end(arglist);
        return;
    }

    va_start(arglist, fmt);
    vdprintf(l.fd, log_fmt, arglist);
    va_end(arglist);
}

void
log_set_level(enum log_levels level)
{
    l.level = level;
}

void
log_initialize(const char *path, enum log_levels level)
{

    char log_path[1024] = {0};
    sprintf(log_path, "%s/memqueue.log", path);

    l.level = level;
    l.fd = open(log_path, O_CREAT | O_APPEND | O_WRONLY, 0640);
    if (l.fd < 0) {
        warn("Cannot open file %s for logging", log_path);
        l.fd = 2; /* stderr */
    }
}
