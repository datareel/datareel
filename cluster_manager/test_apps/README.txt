DR Cluster Manager application resource testing

Test code for cluster application tesing in a 4 node cluster. To use
for DRCM testing:

From this directrory as the user you want to run the apps as:

$ mkdir ~/test_apps
$ cp *.sh ~/test_apps/.
$ chmod 755 ~/test_apps/*.sh

NOTE: On all your cluster node the user must have a copy ~/test_apps
directory. If the user's home directory is not NFS mounted you will
need to copy the ~/test_apps to all cluster nodes. 

Edit your "/etc/drcm/cm.cfg" file and add the following to the global
application resource section:

[CM_APPLICATIONS]
app1, root:root, ~/test_apps/start_app1.sh, ~/test_apps/stop_app1.sh, ~/test_apps/ensure_app1.sh
app2, root:root, ~/test_apps/start_app2.sh, ~/test_apps/stop_app2.sh, ~/test_apps/ensure_app2.sh
app3, root:root, ~/test_apps/start_app3.sh, ~/test_apps/stop_app3.sh, ~/test_apps/ensure_app3.sh
app4, root:root, ~/test_apps/start_app4.sh, ~/test_apps/stop_app4.sh, ~/test_apps/ensure_app4.sh

NOTE: Change the root user and group to the user and group that has
the ~/test_apps directory. 

Add the following application lines to your CM_NODE sections:

[CM_NODE]
nodename = cmnode1
...
node_applications = app1
node_backup_applications = cmnode2:app2, cmnode3:app3, cmnode4:app4 
 
[CM_NODE]
nodename = cmnode2
...
node_applications = app2
node_backup_applications = cmnode1:app1, cmnode3:app3, cmnode4:app4  

[CM_NODE]
nodename = cmnode3
...
node_applications = app3
node_backup_applications = cmnode1:app1, cmnode2:app2, cmnode4:app4    

[CM_NODE]
nodename = cmnode4
...
node_applications = app4
node_backup_applications = cmnode1:app1, cmnode2:app2, cmnode3:app3      

NOTE: Always test the cm.cfg file after any updates. On the cluster
node where you made the cm.cfg file changes:

/usr/sbin/drcm_server --check-config

Copy the testing "/etc/drcm/cm.cfg" file to all CM nodes and restart the
drcm_server on all nodes.

To check the application status, from on of your cluster nodes:

> /usr/sbin/drcm_server --client --command=cm_stat

To test application fail over and fail back, stop the drcm_server on 1
or more nodes. Check the cm_stat and restart the drcm_server to
monitor the fail back.
