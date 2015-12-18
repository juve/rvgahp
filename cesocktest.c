#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    int socks[2];

    socketpair(PF_LOCAL, SOCK_STREAM, 0, socks);

    int gahp_sock = socks[0];
    int ssh_sock = socks[1];

    pid_t ssh_pid = fork();
    if (ssh_pid == 0) {
        int orig_err = dup(STDERR_FILENO);
        close(gahp_sock);
        close(0);
        close(1);
        close(2);
        dup(ssh_sock);
        dup(ssh_sock);
        dup(ssh_sock);
        execl("/bin/sh", "/bin/sh", "-c", "/usr/bin/nc -l 12345", NULL);
        dprintf(orig_err, "ERROR execing NC\n");
        _exit(1);
    } else if (ssh_pid < 0) {
        fprintf(stderr, "ERROR launching NC\n");
        exit(1);
    }

    /* Now that the ssh process is forked, we can close its socket */
    close(ssh_sock);

    char buf[BUFSIZ];
    size_t b = read(gahp_sock, buf, BUFSIZ);
    buf[b] = '\0';
    printf("GOT %s\n", buf);

    /* Fork GAHP */
    pid_t gahp_pid = fork();
    if (gahp_pid == 0) {
        int orig_err = dup(STDERR_FILENO);
        close(0);
        close(1);
        close(2);
        dup(gahp_sock);
        dup(gahp_sock);
        dup(gahp_sock);
        execl("/bin/sh", "/bin/sh", "-c", "GLITE_LOCATION=/usr/local/BLAH /usr/local/BLAH/bin/blahpd", NULL);
        dprintf(orig_err, "ERROR execing GAHP\n");
        _exit(1);
    } else if (gahp_pid < 0) {
        fprintf(stderr, "ERROR launching GAHP\n");
        exit(1);
    }

    /* Now we can close the gahp socket */
    close(gahp_sock);

    printf("Waiting for processes\n");

    int ssh_status, gahp_status;
    waitpid(ssh_pid, &ssh_status, 0);
    printf("NC exited with %d\n", ssh_status);
    waitpid(gahp_pid, &gahp_status, 0);
    printf("GAHP exited with %d\n", ssh_status);

    exit(0);
}
