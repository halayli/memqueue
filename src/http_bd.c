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

  http_bd.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <lthread.h>

#include "sock_easy.h"

struct cmd_opt {
    char *recvd;
    int    len;
    int fd;
};
typedef struct cmd_opt cmd_opt_t;

#define NELEMENTS(x) (sizeof (x) / sizeof x[0])

void cmd_exit(struct cmd_opt *cmd);
void cmd_help(struct cmd_opt *cmd);
void cmd_man(struct cmd_opt *cmd);
void cmd_ls(struct cmd_opt *cmd);
void cmd_log(struct cmd_opt *cmd);

int bd_cmd_add(char *cmd, char *arg, char *desc, void (*f)(cmd_opt_t *));

struct _cmd{
    char cmd[256];
    void (*func)(cmd_opt_t *);
    char desc[256];
    char arg[256];
    char *help;
} cmds[256] =  {
    /* help */
    {"help", cmd_help,
     "print backdoor commands.", {0}, NULL},
    /* man */
    {"man", cmd_man,
     "print command description.", {0}, NULL},
    /* exit */
    {"exit", cmd_exit,
     "exit backdoor.", {0}, NULL},
    /* quit */
    {"quit", cmd_exit,
     "alias for exit.", {0}, NULL},
    /* ls */
    {"ls", cmd_ls,
     "list proxy and thread objects. `man ls` for more.", {0}, NULL},
    /* log */
    {"log", cmd_log,
     "show/set proxy log levels.", {0}, NULL},
    {{0, 0, 0, 0, 0}}
};

int
is_whitespace(char *s)
{
    while(*s) {
        if(!isspace(*s))
            return 0;
        s++;
    }
    return 1;
}

int
bd_cmd_add(char *cmd, char *arg, char *desc, void (*f)(cmd_opt_t *))
{
    int i;

    if (strlen(cmd) > 255 || strlen(arg) > 255 || strlen(desc) > 255)
        return -1;

    for (i = 0; i < NELEMENTS(cmds) && cmds[i].func; i++);

    if ((i + 1) == 256)
        return -1;

    strcpy(cmds[i].cmd, cmd);
    strcpy(cmds[i].arg, arg);
    strcpy(cmds[i].desc, desc);
    cmds[i].func = f;

    cmds[i + 1].func = NULL;

    return 0;
}

void
get_cmd(struct _cmd *cmd, char *buff, int len)
{
    int i, arg_len = 0, cmd_len = 0;
    char *line_end, *arg = NULL;

    cmd->func = NULL;
    bzero(cmd->cmd, sizeof(cmd->cmd));
    bzero(cmd->arg, sizeof(cmd->arg));
    line_end = strchr(buff, '\n');
    if (!line_end)
        return;

    buff[len] = '\0';
    if ((arg = strchr(buff, ' ')) != NULL) {
        cmd_len = arg - buff;
        while (isspace(*arg) && (++arg < line_end));
        arg_len = line_end - arg;
    } else {
        cmd_len = line_end - buff - 1;
    }

    for(i = 0; i < NELEMENTS(cmds); i++) {
        if (strncmp(cmds[i].cmd, buff, cmd_len) == 0) {
            cmd->func = cmds[i].func;
            strncpy(cmd->cmd, cmds[i].cmd, 256);
            if (strncmp(cmds[i].arg, arg, arg_len) == 0)
                strncpy(cmd->arg, arg, arg_len);
            break;
        }
    }
}

void
bd_lt_cli(lthread_t *lt, int fd)
{
    char buff[1024] = {0};
    int ret;
    struct _cmd cmd;
    struct cmd_opt cmd_options;
    char *err = "Unknown Command\n";

    DEFINE_LTHREAD;

    sprintf(buff, 
        "Copyright (c) 2011 express backdoor\n\nlthread 1.0\n"
        "Play Nice.\n");

    lthread_send(fd, buff, strlen(buff), 0);

    while (1) {
        bzero(buff, sizeof buff);
        ret = lthread_send(fd, "%", strlen("%"), 0);
        if (ret <= 0)
            break;
        ret = lthread_recv(fd, buff, 1024, 0, 0);
        if (ret <= 0)
            break;

        if(is_whitespace(buff))
            continue;

        get_cmd(&cmd, buff, ret);
        if (cmd.func == NULL) {
            lthread_send(fd, err, strlen(err), 0);
            continue;
        }

        cmd_options.fd = fd;
        cmd_options.recvd = buff;
        cmd_options.len = ret;
        (*cmd.func)(&cmd_options);
    }

    close(fd);
}

void
bd_lt_listener(lthread_t *lt, int args)
{
    lthread_t *lt_new;
    socklen_t addrlen;
    int s_fd, c_fd;
    struct   sockaddr_in cin;

    DEFINE_LTHREAD;
    s_fd = e_listener("0.0.0.0", 5557);
    if (s_fd == -1) {
        perror("Cannot listen on socket");
        return;
    }

    while (1) {
        c_fd = lthread_accept(s_fd, (struct sockaddr *)&cin, &addrlen);

        if (!c_fd) {
            break;
        }

        lthread_create(&lt_new, bd_lt_cli, (void *)(long)c_fd);
    }
}

void
cmd_exit(struct cmd_opt *cmd)
{
    close(cmd->fd);
}

void
cmd_man(cmd_opt_t *cmd)
{
    lthread_send(cmd->fd, "man", 3, 0);

    return;
}

void
cmd_ls(cmd_opt_t *cmd)
{
    char *tmp = lthread_summary();
    lthread_send(cmd->fd, tmp, strlen(tmp), 0);
    free(tmp);

    return;
}

void
cmd_log(cmd_opt_t *cmd)
{
    lthread_send(cmd->fd, "log", 3, 0);

    return;
}

void
cmd_help(struct cmd_opt *cmd)
{
    int i, len = 1024;
    char resp[1024];

    snprintf(resp, len, "lthread backdoor commands\n");
    snprintf(&resp[strlen(resp)], len - strlen(resp),
        "  %-8s  %s\n", "cmd", "desc");
    for(i = 0; i < NELEMENTS(cmds) && cmds[i].func; i++) {
        snprintf(&resp[strlen(resp)], len - strlen(resp),
             "  %-8s", cmds[i].cmd);
        snprintf(&resp[strlen(resp)], len - strlen(resp),
            "  %s\n", cmds[i].desc);
        if (lthread_send(cmd->fd, resp, strlen(resp), 0) == -1)
            return;
        bzero(resp, sizeof resp);
    }
}
