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

#include "condor_config.h"

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s [-h HOST] [-p PORT]\n", argv0);
    fprintf(stderr, "\t-h HOST\tHost where the reverse GAHP broker is running\n");
    fprintf(stderr, "\t-p PORT\tPort of the reverse GAHP broker\n");
}

int main(int argc, char** argv) {
    argv0 = basename(strdup(argv[0]));
    char *server = "127.0.0.1";
    unsigned short port = 41000;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "hs:p:")) != -1) {
        switch (c) {
            case 'h':
                usage();
                return 1;
                break;
            case 's':
                server = optarg;
                break;
            case 'p':
                if (sscanf(optarg, "%hu", &port) != 1) {
                    fprintf(stderr, "Invalid port: %s\n", optarg);
                    return 1;
                }
                break;
            case '?':
                if (optopt == 's' || optopt == 'p') {
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                } else {
                    fprintf (stderr, "Unknown option '-%c'.\n", optopt);
                }
                return 1;
            default:
                abort();
        }
    }

    if (optind != argc) {
        fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
        usage();
        exit(1);
    }

    fprintf(stdout, "%s starting...\n", argv0);
    fprintf(stdout, "server: %s:%hu\n", server, port);

    while (1) {
        struct sockaddr_in this_addr, serv_addr;
        int addrlen = sizeof(struct sockaddr_in);
        memset(&this_addr, 0, addrlen);
        memset(&serv_addr, 0, addrlen);

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, server, &serv_addr.sin_addr);

        int sck = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

        if (connect(sck, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            /* If we didn't connect, then wait a few seconds and try again */
            sleep(10);
            continue;
        }

        printf("Connected to rvgahp_proxy\n");

        /* Get the name of the GAHP to launch from the reverse_gahp */
        char gahp[BUFSIZ];
        int sz = read(sck, gahp, BUFSIZ);
        if (sz <= 0) {
            fprintf(stderr, "ERROR reading gahp name from rvgahp_proxy\n");
            close(sck);
            continue;
        }
        gahp[sz] = '\0';

        printf("Launching GAHP: %s\n", gahp);

        /* Construct the actual GAHP command */
        /* TODO Get these from condor_config_val */
        char *gahp_command;
        if (strncmp("batch_gahp", gahp, 10) == 0) {
            gahp_command = "BLAHPD_LOCATION=/usr/local/BLAH /usr/local/BLAH/bin/blahpd";
        } else if (strncmp("condor_ft-gahp", gahp, 14) == 0) {
            gahp_command = "_condor_BOSCO_SANDBOX_DIR=~/.condor /usr/sbin/condor_ft-gahp -f";
        } else {
            fprintf(stderr, "ERROR: Unknown GAHP: %s\n", gahp);
            close(sck);
            continue;
        }
        printf("Actual GAHP command: %s\n", gahp_command);

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

        /* We no longer need the client socket */
        close(sck);
    }

    exit(0);
}

