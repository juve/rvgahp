#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "condor_config.h"

int condor_config_val(char *var, char *val, size_t valsize, const char *default_val) {
    char cmd[BUFSIZ];
    snprintf(cmd, BUFSIZ, "condor_config_val %s", var);

    FILE *proc = popen(cmd, "r");
    if (proc == NULL) {
        fprintf(stderr, "ERROR getting %s config var\n", var);
        return -1;
    }

    if (fgets(val, valsize, proc) == NULL) {
        fprintf(stderr, "ERROR reading output of condor_config_val\n");
        pclose(proc);
        return -1;
    }

    int status = pclose(proc);

    if (strncmp("Not defined:", val, 12) == 0) {
        if (default_val == NULL) {
            return -1;
        }
        snprintf(val, valsize, "%s", default_val);
        return 0;
    }

    if (status != 0) {
        fprintf(stderr, "ERROR reading condor_config_val %s:\n", var);
        fprintf(stderr, "%s", val);
        return -1;
    }

    int sz = strlen(val) - 1;
    while (val[sz] == '\n' || val[sz] == '\r') {
        val[sz] = '\0';
        sz--;
    }

    return 0;
}

