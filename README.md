Reverse GAHP
============

This is an implementation of the remote_gahp for Condor that doesn't require
ssh connections to the remote resource. Instead, it uses connection brokering
to establish bi-directional communication between the GridManager and GAHP
processes running on the remote resource.

Configuration
-------------

On your submit host:

set REMOTE_GAHP = /path/to/rvgahp_proxy

On the remote resource:

start the rvgahp_ce process.
