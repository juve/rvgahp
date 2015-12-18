Reverse GAHP
============

This is an implementation of the remote_gahp for Condor that doesn't require
ssh connections to the remote resource. Instead, it uses connection brokering
to establish bi-directional communication between the GridManager and GAHP
processes running on the remote resource. The key benefit of this approach is
that it enables remote job submission without requiring the remote resource
to run services that accept incoming network connections (either because of
security policy, or because of a firewall or something).

It works like this:

![rvgahp design](doc/rvgahp.png)

The rvgahp_ce uses SSH to establish a secure connection to the submit host.
The SSH connection starts a helper process that listens on a UNIX domain
socket for connections. If the SSH session gets disconnected, the CE
immediately reconnects. When a remote GAHP job is submitted, the GridManager
launches an rvgahp_proxy process to communicate with the GAHP servers. The
proxy connects to the helper and sends the name of the GAHP to start (batch_gahp
for job submission or condor_ft-gahp for file transfer). The helper forwards
the request to the CE, which forks the GAHP and connects it to the SSH process
using a socketpair. Once this is done, it immediately establishes another
SSH connection to the submit host. The proxy copies its stdin from the
GridManager to the helper, and data from the helper to stdout and back to the
GridManager. The helper passes data to the SSH process, and the SSH process
passes data to the GAHP. Once all connections are established, the job
execution proceeds. When the GridManager is done, the GAHP servers exit and
the connections are torn down.

Configuration
-------------

On your submit host:

1. In your condor_config set:

    ```
    REMOTE_GAHP = /path/to/rvgahp_proxy
    ```

On the remote resource:

1. Make sure the batch_gahp/glite/blahp is installed and configured correctly
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

    RVGAHP_CONNECTION = user@example.com

    # Name of the CE (needs to match grid_resource from job)
    RVGAHP_CE_NAME = hpcc
    ```

1. Start the rvgahp_ce process.

Example Job
-----------
```
universe = grid

grid_resource = batch pbs hpcc
+remote_cerequirements = EXTRA_ARGUMENTS=="-N testjob -l walltime=00:01:00 -l nodes=1:ppn=1"

executable = /bin/date
transfer_executable = False

output = test_$(cluster).$(process).out
error = test_$(cluster).$(process).err
log = test_$(cluster).$(process).log

should_transfer_files = YES
when_to_transfer_output = ON_EXIT
copy_to_spool = false
notification = NEVER

queue 1
```
