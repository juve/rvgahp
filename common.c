/**
 * Copyright 2015 University of Southern California
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include "common.h"

char *timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t t = tv.tv_sec;

    struct tm tm;
    localtime_r(&t, &tm);

    static char ts[32];
    snprintf(ts, 32, "%d-%d-%d %02d:%02d:%02d.%06lu",
            1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec);

    return ts;
}

static int _condor_config_val(char *var, char *val, size_t valsize, const char *default_val) {
    char cmd[BUFSIZ];
    snprintf(cmd, BUFSIZ, "condor_config_val %s 2>&1", var);

    FILE *proc = popen(cmd, "r");
    if (proc == NULL) {
        fprintf(stderr, "ERROR getting %s config var\n", var);
        return -1;
    }

    char buf[BUFSIZ];
    if (fgets(buf, BUFSIZ, proc) == NULL) {
        fprintf(stderr, "ERROR reading output of condor_config_val\n");
        pclose(proc);
        return -1;
    }

    int status = pclose(proc);
    int saverrno = errno;

    if (strncmp("Not defined:", buf, 12) == 0) {
        if (default_val == NULL) {
            return -1;
        }
        snprintf(val, valsize, "%s", default_val);
        return 0;
    }

    if (status != 0) {
        fprintf(stderr, "ERROR reading condor_config_val %s: %s\n", var, strerror(saverrno));
        fprintf(stderr, "%s", buf);
        return -1;
    }

    /* Trim the string */
    int sz = strlen(buf) - 1;
    while (buf[sz] == '\n' || buf[sz] == '\r') {
        buf[sz] = '\0';
        sz--;
    }

    snprintf(val, valsize, "%s", buf);

    return 0;
}

int condor_config_val(char *var, char *val, size_t valsize, const char *default_val) {
    /* Need to restore the default handler for this function so that wait4()
     * and, as a result, pclose() work correctly. */
    void *sigchld_handler = signal(SIGCHLD, SIG_DFL);
    int result = _condor_config_val(var, val, valsize, default_val);
    signal(SIGCHLD, sigchld_handler);
    return result;
}

