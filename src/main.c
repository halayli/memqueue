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

  main.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <err.h>
#include <sys/resource.h>

#include <arpa/inet.h>
#include <netdb.h>

#ifdef __FreeBSD__
#include <sys/rtprio.h>
#endif

#include <sys/queue.h>
#include "http_str.h"
#include "http.h"
#include "http_bd.h"
#include "sock_easy.h"
#include "log.h"
#include "time.h"
#include "router.h"
#include "memqueue.h"
#define VERSION "1.0"

#include <sys/wait.h>

static int
handle_args (int argc, char **argv, lsn_t *lsn)
{
    extern char *optarg;
    int c = 0;
    char *errmsg = NULL;
    int err = 0;

    const char *menu = \
    "usage: express [options]\n"
    "-p <port>  : http listener port (default: 5556)\n"
    "-a <ip>    : http listener IP address (default: 0.0.0.0)\n"
    "-i <eth>   : http listener interface\n"
    "-h         : print this help message and exits\n"
    "-l <level> : set log level to info|error|debug|trace\n";

    lsn->lsn_port = 5556;

    while ((c = getopt(argc, argv, "l:hp:")) != -1) {
        switch(c) {
        case 'p':
            lsn->lsn_port = strtol(optarg, NULL, 10);
            if (lsn->lsn_port < 1) {
                errmsg = "port range is between 1 and 65535";
                err = -1;
                goto done;
            }
            break;
        case 'l':
            /*
            if (strcasecmp(optarg, "error") == 0)
                lsn->http_log_level = SRV_ERR;
            else if (strcasecmp(optarg, "warn") == 0)
                lsn->http_log_level = SRV_WRN;
            else if (strcasecmp(optarg, "trace") == 0)
                lsn->http_log_level = SRV_TRC;
            else if (strcasecmp(optarg, "debug") == 0)
                lsn->http_log_level = SRV_DBG;
            else if (strcasecmp(optarg, "info") == 0)
                lsn->http_log_level = SRV_INFO;
            else
                lsn->http_log_level = 0;
            */
            break;
        case 'h':
        default:
            err = -1;
            goto done;
        }
    }

done:
    if (err == -1) {
        printf("%s", menu);
        if (errmsg)
            printf("ERROR: %s\n", errmsg);
        return err;
    }

    return 0;
}

int lsn_fd = 0;

int
main(int argc, char *argv[])
{

    lsn_t lsn = {0};
    struct sigaction act;
    act.sa_handler = SIG_IGN;

    /* the process by default exits on SIGPIPE, we don't want that */
    signal(SIGPIPE, SIG_IGN);
    sigaction(SIGPIPE, &act, NULL);

    if (handle_args(argc, argv, &lsn) == -1)
        exit(1);

#ifdef __FreeBSD__
    struct rlimit limit;
    struct rtprio prio = {.type = RTP_PRIO_REALTIME, .prio = 0};
    limit.rlim_cur = limit.rlim_max = 30000;

    if (rtprio(RTP_SET, getpid(), &prio) == -1)
        perror("Warning: Failed to set rtprio");

    if (setrlimit(RLIMIT_NOFILE, &limit) == -1)
        perror("Cannot set maximum file limit");

    limit.rlim_cur = limit.rlim_max = 1024*1024*2000;

    if (setrlimit(RLIMIT_AS, &limit) == -1)
        perror("Cannot set maximum file limit");

    if (setrlimit(RLIMIT_DATA, &limit) == -1)
        perror("Cannot set maximum file limit");

    if (setrlimit(RLIMIT_SBSIZE, &limit) == -1)
        perror("Cannot set maximum file limit");
#endif

    http_route_init();
    if (lsn_init(&lsn, http_route_handle_request, "/tmp", "memqueue") != 0)
        exit(1);

    
    lsn_fd = e_listener("127.0.0.1", lsn.lsn_port);
    /*
    int pid = 0;
    int i = 0;
    for (i = 0; i < 1; i++) {
        pid = fork();
        printf("forking %d\n", pid);
        if (pid == 0) {
            memqueue_init();
            lsn_run(&lsn);
        }
    }

    if (pid)
        waitpid(pid, NULL, 0);
    */
    memqueue_init();
    lsn_run(&lsn);

    printf("Exiting...! %d\n", getpid());

    return 0;
}
