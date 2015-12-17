#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <libgen.h>

#include "common.h"

#define BROKER_BANNER "rvgahp_broker server"

struct client {
    int sock;
    char *name;
    struct client *next;
    struct client *prev;
};

struct client *clients = NULL;
int nclients = 0;

char *argv0;

struct client *add_client(int sock) {
    struct client *c = (struct client *)malloc(sizeof(struct client));
    if (c == NULL) {
        fprintf(stderr, "ERROR allocating new client: %s\n", strerror(errno));
        exit(1);
    }
    c->sock = sock;
    c->name = NULL;
    c->next = NULL;
    c->prev = NULL;

    if (clients == NULL) {
        clients = c;
    } else {
        /* Add it to the end */
        struct client *p;
        for (p = clients; p->next != NULL; p = p->next);
        c->prev = p;
        p->next = c;
    }

    nclients++;

    return c;
}

struct client *find_client(int sock) {
    log("Looking for %d\n", sock);
    for (struct client *p = clients; p != NULL; p = p->next) {
        if (p->sock == sock) {
            return p;
        }
    }
    return NULL;
}

void remove_client(int sock) {
    log("Hanging up on client %d\n", sock);
    close(sock);

    struct client *p = find_client(sock);
    if (p == NULL) {
        return;
    }
    if (p->next != NULL) {
        /* Not at the end of the list */
        p->next->prev = p->prev;
    }
    if (p->prev == NULL) {
        /* At the start of the list */
        clients = p->next;
    } else {
        /* Not at the start of the list */
        p->prev->next = p->next;
    }
    if (p->next == NULL && p->prev == NULL) {
        /* The list is empty now */
        clients = NULL;
    }
    if (p->name != NULL) {
        free(p->name);
    }
    free(p);
    p = NULL;
    nclients--;
}

void usage() {
    fprintf(stderr, "Usage: %s\n", argv0);
    fprintf(stderr, "This command has no options\n");
}

void handle_connection(struct client *c) {
    log("Got connection\n");
    /* Print HELLO banner */
    dprintf(c->sock, "%s\r\n", BROKER_BANNER);
}

void handle_request(struct client *c) {
    char buf[BUFSIZ];
    int read = recv(c->sock, buf, BUFSIZ-1, 0);
    if (read < 0) {
        fprintf(stderr, "ERROR reading from client: %s\n", strerror(errno));
        remove_client(c->sock);
        return;
    }
    if (read == 0) {
        return;
    }
    if (read < 2) {
        fprintf(stderr, "Invalid request\n");
        remove_client(c->sock);
        return;
    }
    if (buf[read-2] == '\r' && buf[read-1] == '\n') {
        buf[read-2] = '\0';
        buf[read-1] = '\0';
    } else {
        fprintf(stderr, "Invalid command terminator");
        remove_client(c->sock);
        return;
    }
    if (strlen(buf) < 4) {
        fprintf(stderr, "Invalid command: %s\n", buf);
        remove_client(c->sock);
        return;
    }
    if (strncasecmp(buf, "QUIT", 4) == 0) {
        dprintf(c->sock, "S\r\n");
        remove_client(c->sock);
    } else if (strncasecmp(buf, "COMMANDS", 8) == 0) {
        dprintf(c->sock, "S QUIT COMMANDS REGISTER LAUNCH_GAHP LIST_CES\r\n");
    } else if (strncasecmp(buf, "REGISTER", 8) == 0) {
        /* TODO Handle register command */
        /* Format: REGISTER <SP> NAME <CRLF> */
        dprintf(c->sock, "E NOT IMPLEMENTED\r\n");
    } else if (strncasecmp(buf, "LAUNCH_GAHP", 11) == 0) {
        /* TODO Handle launch_gahp command */
        /* Format: LAUNCH_GAHP <SP> NAME <SP> ADDRESS <SP> GAHP <CRLF> */
        dprintf(c->sock, "E NOT IMPLEMENTED\r\n");
    } else if (strncasecmp(buf, "LIST_CES", 8) == 0) {
        for (struct client *p = clients; p != NULL; p = p->next) {
            dprintf(c->sock, "S");
            if (p->name != NULL) {
                dprintf(c->sock, " %s", p->name);
            }
            dprintf(c->sock, "\r\n");
        }
    } else {
        dprintf(c->sock, "E INVALID COMMAND\r\n");
        remove_client(c->sock);
    }
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc > 1) {
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        usage();
        exit(1);
    }

    log("Starting...\n");

    char port_str[10];
    if (condor_config_val("RVGAHP_BROKER_PORT", port_str, 10, DEFAULT_BROKER_PORT) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_BROKER_PORT from config\n");
        exit(1);
    }
    unsigned short port;
    if (sscanf(port_str, "%hu", &port) != 1) {
        fprintf(stderr, "ERROR Invalid port: %s\n", port_str);
        exit(1);
    }
    log("Port: %hu\n", port);

    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in this_addr, peer_addr;
    memset(&this_addr, 0, addrlen);
    memset(&peer_addr, 0, addrlen);

    this_addr.sin_port = htons(port);
    this_addr.sin_family = AF_INET;
    this_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sck = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sck < 0) {
        fprintf(stderr, "ERROR creating socket: %s\n", strerror(errno));
        return 1;
    }

    /*
    int enable = 1;
    if (setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        fprintf(stderr, "ERROR setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
        return 1;
    }
    */

    if (bind(sck, (struct sockaddr *)&this_addr, addrlen) < 0) {
        fprintf(stderr, "ERROR binding socket: %s\n", strerror(errno));
        return 1;
    }

    if (listen(sck, 5) < 0) {
        fprintf(stderr, "ERROR listening on socket: %s\n", strerror(errno));
        return 1;
    }

    struct pollfd *ufds = NULL;
    while (1) {
        if (ufds != NULL) {
            free(ufds);
        }
        ufds = (struct pollfd *)malloc(sizeof(struct pollfd) * (nclients+1));
        ufds[0].fd = sck;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;

        int i = 1;
        for (struct client *c = clients; c != NULL; c = c->next) {
            ufds[i].fd = c->sock;
            ufds[i].events = POLLIN;
            ufds[i].revents = 0;
            i++;
        }

        int rv = poll(ufds, nclients+1, -1);
        if (rv == -1) {
            fprintf(stderr, "ERROR on poll: %s\n", strerror(errno));
        } else {
            for (int i = 0; i<nclients+1; i++) {
                struct pollfd *p = &(ufds[i]);
                if (p->revents == 0) {
                    /* No events here, go to the next one */
                    continue;
                }

                /* Do something special for listening socket */
                if (p->fd == sck) {
                    if (p->revents & POLLIN) {
                        int csck = accept(sck, (struct sockaddr *)&peer_addr, &addrlen);
                        if (csck <= 0) {
                            fprintf(stderr, "ERROR accepting connection: %s\n", strerror(errno));
                            continue;
                        }
                        struct client *c = add_client(csck);
                        handle_connection(c);
                    }
                } else {
                    if (p->revents & POLLERR) {
                        remove_client(p->fd);
                        continue;
                    }
                    if (p->revents & POLLIN) {
                        log("Got data from client %d\n", p->fd);
                        struct client *c = find_client(p->fd);
                        handle_request(c);
                    }
                    if (p->revents & POLLHUP) {
                        remove_client(p->fd);
                    }
                }
            }
        }
    }

    close(sck);

    exit(0);
}

