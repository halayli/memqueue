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
#include <inttypes.h>

#include <lthread.h>

#include "http_impl.h"

struct _log {
    char *app_name;
    FILE *log_fp;
    char *file_path;
    int log_level;
};

static char *log_levels[] = {
    [APP_INFO] = "[INFO]: ",
    [APP_WRN] = "[WARN]: ",
    [APP_TRC] = "[TRC]: ",
    [APP_DBG] = "[DBG]: ",
    [APP_ERR] = "[ERR]: ",
    [APP_NOOP] = "",
};

log_t *
app_log_new(log_level_t level, char *path, char *filename)
{

    log_t *log = NULL;
    char *file_path = NULL;

    if ((log = malloc(sizeof(log_t))) == NULL)
        return NULL;

    if (asprintf(&file_path, "%s/%s.log", path, filename)) {
        if ((log->log_fp = fopen(file_path, "a+")) == NULL) {
            fprintf(stderr, "Failed to open log file %s\n", file_path);
            free(file_path);
            return NULL;
        }
    }

    log->file_path = file_path;
    log->log_level = level;

    return log;
}

int
app_log(log_t *log, log_level_t level, char *fmt, ...)
{
    va_list arglist;
    char *new_fmt = NULL;

    if (!log || !log->log_fp)
        return -1;

    asprintf(&new_fmt, "%"PRIu64":%s%s\n", lthread_id(), log_levels[level], fmt);

    va_start(arglist, fmt);
    vfprintf(log->log_fp, new_fmt, arglist);
    fflush(log->log_fp);
    free(new_fmt);
    va_end(arglist);

    return 0;
}
