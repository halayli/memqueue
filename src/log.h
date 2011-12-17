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

  log.h
*/
#ifndef _HTTP_LOG_H_
#define _HTTP_LOG_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/*
 * logger functionalities
 */
typedef enum {
    APP_INFO,
    APP_WRN,
    APP_TRC,
    APP_DBG,
    APP_ERR,
    APP_NOOP,
} log_level_t;

struct _log;
typedef struct _log log_t;

int app_log(log_t *log, log_level_t level, char *fmt, ...);
log_t * app_log_new(log_level_t level, char *path, char *filename);

#endif
