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
#ifndef LOG_H_
#define LOG_H_
#include <stdarg.h>

/*
 * logger functionalities
 */
enum log_levels {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NOOP
};

void log_(enum log_levels level, const char *module,
    const char *fmt, ...);

void log_initialize(const char *path, enum log_levels level);

void log_set_level(enum log_levels level);

#define LOG(level, fmt, ...)                 \
do {                                         \
    log_(level, __FILE__, fmt, ##__VA_ARGS__); \
} while (0)                                  \

#define LOG_WARN(fmt, ...)                   \
    LOG(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)  \

#define LOG_ERROR(fmt, ...)                  \
    LOG(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__) \

#define LOG_TRACE(fmt, ...)                  \
    LOG(LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__) \

#define LOG_INFO(fmt, ...)                   \
    LOG(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)  \

#endif
