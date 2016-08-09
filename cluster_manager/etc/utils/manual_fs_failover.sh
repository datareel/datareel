#!/bin/bash

if [ -f ~/.bashrc ]; then . ~/.bashrc; fi

if [ -z "${CFGFILE}" ]; then export CFGFILE=/etc/drcm/my_cluster_conf/my_cluster_info.sh; fi
. ${CFGFILE}
if [ -z "${DRCM_server}" ]; then export DRCM_server="/usr/sbin/drcm_server"; fi
if [ -z "${PRIMARY_FILESYSTEMS_IP}" ]; then echo "ERROR - 2-way FS config not set"; exit 1; fi
if [ -z "${BACKUP_FILESYSTEMS_IP}" ]; then echo "ERROR - 2-way FS config not set"; exit 1; fi
if [ -z "${BACKUP_USER}" ]; then echo "ERROR - 2-way FS config not set"; exit 1; fi

isbackup=$(/etc/drcm/utils/is_backup_fs.sh)
if [ "${isbackup}" == "TRUE" ]; then 
    echo "INFO - We are on the backup node, no action taken"
    exit 0
fi

hasfailedover=$(/etc/drcm/utils/fs_has_failedover.sh )

if [ "${hasfailedover}" == "TRUE" ]; then
    echo "ERROR - Cluster has failed over, no action taken"
    exit 1
fi

if [ "${hasfailedover}" == "ERROR" ]; then
    echo "ERROR - Cluster is in unknown state, no action taken"
    exit 1
fi

echo "Failing over DRCM cluster and 2-way file system sync to backup node"

USERNAME=$(whoami)

if [ "${USERNAME}" != "${BACKUP_USER}" ]
    then
    echo "You must be ${USERNAME} to run this script"
    exit 1
fi

echo -n "Are you sure you want to continue (yes/no)> "
read prompt
if [ "${prompt}" != "yes" ]
    then
    echo "Exiting script with no action taken"
    exit 1
fi

echo "You have selected to fail over this cluster"
echo "Backing up 2-way file systems from primary to backup node"

/etc/drcm/file_system_sync/sync_mysql.sh
/etc/drcm/file_system_sync/sync_pgsql.sh
/etc/drcm/file_system_sync/sync_testfs.sh
/etc/drcm/file_system_sync/sync_cifs.sh
/etc/drcm/file_system_sync/sync_nfs.sh
/etc/drcm/file_system_sync/sync_www.sh
/etc/drcm/file_system_sync/sync_other.sh

echo "Stopping the DRCM cluster service"

if [ ! -d /etc/systemd ]; then 
    service drcm_server stop
else 
    systemctl stop drcm_server
fi

echo ""
echo "The DRCM cluster and 2-way file systems have failed over to secondary node"
echo "To fail back to primary, restart the drcm_server service on this node"
echo ""

exit 0
