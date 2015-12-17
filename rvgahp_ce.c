#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <libgen.h>
#include <netdb.h>
#include <time.h>

#include "condor_config.h"

#define log(fmt, ...) \
    fprintf(stdout, "%s %s[%d]: " fmt, ts(), argv0, \
            getpid(), ##__VA_ARGS__)

char *argv0 = NULL;

char *ts() {
    time_t t;
    time(&t);

    struct tm tm;
    localtime_r(&t, &tm);

    static char timestamp[1024];
    snprintf(timestamp, 1024, "%d-%d-%d %02d:%02d:%02d",
            1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    return timestamp;
}

void usage() {
    fprintf(stderr, "Usage: %s\n", argv0);
    fprintf(stderr, "This command has no options\n");
}

int set_condor_config() {
    char *homedir = getenv("HOME");
    if (homedir == NULL) {
        fprintf(stderr, "ERROR HOME is not set in environment");
        return -1;
    }

    char config_file[BUFSIZ];
    snprintf(config_file, BUFSIZ, "%s/%s", homedir, ".rvgahp/condor_config.rvgahp");
    if (access(config_file, R_OK) < 0) {
        fprintf(stderr, "ERROR Cannot find config file %s", config_file);
        return -1;
    }

    setenv("CONDOR_CONFIG", config_file, 1);
    return 0;
}

int main(int argc, char** argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc > 1) {
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        usage();
        exit(1);
    }

    log("%s starting...\n", argv0);

    /* Set up configuration */
    if (set_condor_config() < 0) {
        exit(1);
    }
    log("Config file: %s\n", getenv("CONDOR_CONFIG"));

    char server[1024];
    if (condor_config_val("RVGAHP_BROKER_HOST", server, 1024, DEFAULT_BROKER_HOST) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_BROKER_HOST from config file\n");
        exit(1);
    }

    char port[10];
    if (condor_config_val("RVGAHP_BROKER_PORT", port, 10, DEFAULT_BROKER_PORT) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_BROKER_PORT from config file\n");
        exit(1);
    }
    log("Proxy Address: %s:%s\n", server, port);

    char interval_str[10];
    if (condor_config_val("RVGAHP_CE_INTERVAL", interval_str, 10, DEFAULT_INTERVAL) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_CE_INTERVAL from config file\n");
        exit(1);
    }
    int interval;
    if (sscanf(interval_str, "%d", &interval) != 1) {
        fprintf(stderr, "ERROR Invalid RVGAHP_CE_INTERVAL: %s\n", interval_str);
        exit(1);
    }
    log("Polling Interval: %d seconds\n", interval);

    char name[BUFSIZ];
    if (condor_config_val("RVGAHP_CE_NAME", name, BUFSIZ, NULL) != 0) {
        fprintf(stderr, "ERROR Unable to read RVGAHP_CE_NAME from config file\n");
        exit(1);
    }
    log("CE Name: %s\n", name);

    while (1) {
        struct addrinfo hints;
        struct addrinfo *servinfo;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        // get ready to connect
        int gairv = getaddrinfo(server, port, &hints, &servinfo);
        if (getaddrinfo(server, port, &hints, &servinfo)) {
            fprintf(stderr, "ERROR getaddrinfo: %s\n", gai_strerror(gairv));
            goto next;
        }

        /* Try any and all addresses */
        int sck;
        struct addrinfo *ai;
        for (ai = servinfo; ai != NULL; ai = ai->ai_next) {
            sck = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (sck == -1) {
                fprintf(stderr, "error creating socket: %s\n", strerror(errno));
                continue;
            }

            if (connect(sck, ai->ai_addr, ai->ai_addrlen) == -1) {
                close(sck);
                continue;
            }

            /* If we made it this far, then we are connected */
            break;
        }

        freeaddrinfo(servinfo);

        if (ai == NULL) {
            /* We failed to connect, so wait a few and try again */
            goto next;
        }

        log("Connected to rvgahp_proxy\n");

        /* Process the request */
        char message[BUFSIZ];
        int sz = read(sck, message, BUFSIZ);
        if (sz <= 0) {
            fprintf(stderr, "ERROR reading message from rvgahp_proxy\n");
            goto next;
        }
        message[sz] = '\0';

        /* The message format is: ce_name <sp> gahp_name */
        char *ce = message;
        char *gahp = index(message, ' ');
        if (gahp == NULL) {
            fprintf(stderr, "ERROR invalid request: %s\n", message);
            goto next;
        }
        gahp[0] = '\0';
        gahp++;

        /* Make sure that the CE name matches */
        if (strcmp(ce, name) != 0) {
            fprintf(stderr, "ERROR Requested CE does not match: %s != %s\n", ce, name);
            goto next;
        }

        log("Launching GAHP: %s\n", gahp);

        /* Construct the actual GAHP command */
        char gahp_command[BUFSIZ];
        if (strncmp("batch_gahp", gahp, 10) == 0) {
            char batch_gahp[BUFSIZ]; 
            if (condor_config_val("BATCH_GAHP", batch_gahp, BUFSIZ, NULL) < 0) {
                goto next;
            }
            char glite_location[BUFSIZ];
            if (condor_config_val("GLITE_LOCATION", glite_location, BUFSIZ, NULL) < 0) {
                goto next;
            }
            snprintf(gahp_command, BUFSIZ, "GLITE_LOCATION=%s %s", glite_location, batch_gahp);
        } else if (strncmp("condor_ft-gahp", gahp, 14) == 0) {
            char ft_gahp[BUFSIZ];
            if (condor_config_val("FT_GAHP", ft_gahp, BUFSIZ, NULL) < 0) {
                goto next;
            }
            snprintf(gahp_command, BUFSIZ, "%s -f", ft_gahp);
        } else {
            fprintf(stderr, "ERROR: Unknown GAHP: %s\n", gahp);
            goto next;
        }
        log("Actual GAHP command: %s\n", gahp_command);

        /* Double fork to detach GAHP processes */
        pid_t c1 = fork();
        if (c1 > 0) {
            /* Wait for the GAHP process' parent to exit */
            int status;
            waitpid(c1, &status, 0);
            if (status != 0) {
                fprintf(stderr, "ERROR starting GAHP");
            }
        } else if(c1 == 0) {
            /* Make this process the session leader */
            setsid();

            pid_t c2 = fork();
            if (c2 > 0) {
                exit(0);
            } else if (c2 == 0) {
                chdir("/");

                int original_error = dup(2);

                close(0); /* close standard input  */
                close(1); /* close standard output */
                close(2); /* close standard error  */

                if (dup(sck) != 0 || dup(sck) != 1 || dup(sck) != 2) {
                    dprintf(original_error, "ERROR duplicating socket for stdin/stdout/stderr: %s\n", strerror(errno));
                    exit(1);
                }

                execl("/bin/sh", "/bin/sh", "-c", gahp_command, NULL);
                dprintf(original_error, "ERROR launching GAHP: %s\n", strerror(errno));
                _exit(1);
            } else {
                fprintf(stderr, "ERROR forking GAHP process (2): %s\n", strerror(errno));
                exit(1);
            }
        } else {
            fprintf(stderr, "Error forking GAHP process (1): %s\n", strerror(errno));
        }

next:
        /* We no longer need the client socket */
        close(sck);

        /* Wait a few seconds before trying again */
        sleep(interval);
    }

    exit(0);
}

