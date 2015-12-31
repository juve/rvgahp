Reverse GAHP
============

This is an implementation of the remote_gahp for Condor that doesn't require
ssh connections to the remote resource. Instead, it uses connection brokering
to establish bi-directional communication between the GridManager and GAHP
processes running on the remote resource. The key benefit of this approach is
that it enables remote job submission without requiring the remote resource
to run services that accept incoming network connections (usually because of
security policy).

It works like this:

![rvgahp design](doc/rvgahp.png)

The server uses SSH to establish a secure connection to the submit host.
The SSH connection starts a proxy process that listens on a UNIX domain socket
for connections. If the SSH session gets disconnected, the server immediately
reconnects. When a remote GAHP job is submitted, the GridManager launches an
client process to start and communicate with the GAHP servers. The client
connects to the proxy and sends the name of the GAHP to start (batch_gahp for
job submission or condor_ft-gahp for file transfer). The proxy forwards the
request to the server, which forks the appropriate GAHP and connects it to the SSH
process using a socketpair. Once this is done, the server immediately establishes
another SSH connection to the submit host. The client copies its stdin from the
GridManager to the proxy, and data from the proxy to stdout and back to the
GridManager. The proxy passes data to the SSH process, and the SSH process
passes data to the GAHP. Once all connections are established, the job
execution proceeds. When the GridManager is done, the GAHP servers exit and the
connections are torn down.

Configuration
-------------

On your submit host:

1. In your condor_config.local set:

    ```
    REMOTE_GAHP = /path/to/rvgahp_client
    ```
1. (Optional) In your condor_config.local set a throttle for the maximum number of
   submitted jobs. You can set it for all resources using the batch GAHP:

    ```
    GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE_BATCH = 2
    ```

   Or you can set it for a specific resource:

    ```
    # /tmp/user.hpcc.sock is the name of the UNIX socket in the diagram
    # above. It should match the grid_resource from the job and the argument
    # for the rvgahp_helper in the ssh command below.
    GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE = 2, /tmp/user.hpcc.sock
    ```

On the remote resource:

1. Make sure the batch_gahp/glite/blahp is installed and configured correctly.
   This can be tricky to get right, so ask 
   [Pegasus Support](http://pegasus.isi.edu/support) for help if you have
   problems.
1. Create $HOME/.rvgahp
1. Create a $HOME/.rvgahp/condor_config.rvgahp
1. In condor_config.rvgahp set:

    ```
    LIBEXEC = /usr/libexec/condor
    BOSCO_SANDBOX_DIR = $ENV(HOME)/.rvgahp
    LOG = $ENV(HOME)/.rvgahp

    GLITE_LOCATION = $(LIBEXEC)/glite
    BATCH_GAHP = $(GLITE_LOCATION)/bin/batch_gahp

    FT_GAHP_LOG = $(LOG)/FTGahpLog
    FT_GAHP = /usr/sbin/condor_ft-gahp
    ```

1. Create a $HOME/.rvgahp/rvgahp_ssh script like this:

    ```
    #!/bin/bash
    exec ssh -o "ServerAliveInterval 60" -o "BatchMode yes" -i id_rsa_rvgahp user@submithost "/path/to/rvgahp_proxy /tmp/user.hpcc.sock"
    ```

   It is recommended that you create a passwordless ssh key (called id_rsa_rvgahp
   in this example) that can be used to log into your submit host. Test this script
   manually to make sure that it works before continuing to the next step.

1. Start the rvgahp_server process.

Example Condor Job
------------------
The most important keys below are `grid_resource`, which specifies that you want to use the batch GAHP for a PBS cluster and the path to the UNIX socket to use, and `+remote_cerequirements`, which specifies key parameters to pass to the batch GAHP.

```
universe = grid
grid_resource = batch pbs /tmp/user.hpcc.sock
+remote_cerequirements = EXTRA_ARGUMENTS=="-N testjob -l walltime=00:01:00 -l nodes=1:ppn=1"

executable = /bin/date
transfer_executable = False

output = test.out
error = test.err
log = test.log

queue 1
```
