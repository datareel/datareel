#!/bin/bash
# BASH Script
# Operating System(s): RHEL/CENTOS
# Run Level(s): 3, 5
# Shell: BASH shell
# Original Author(s): DataReel Software Development
# File Creation Date: 07/17/2016 
# Date Last Modified: 08/08/2016
#
# Version control: 1.19
#
# Contributor(s):
# ----------------------------------------------------------- 
# ------------- Program Description and Details ------------- 
# ----------------------------------------------------------- 
# This file is part of the DataReel software distribution.
#
# Datareel is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. 
#
# Datareel software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with the DataReel software distribution.  If not, see
# <http://www.gnu.org/licenses/>.
#
# Include script used sync file systems between cluster nodes
#
# NOTE: This script requires a designated backup user to have 
# NOTE: SSH public key authentication to all cluster nodes doing 
# NOTE: file replication between each other. This is a 2-way file
# NOTE: system sync for 2 cluster node to replicate local file
# NOTE: file systems between a primary and a backup node. 
#
# NOTE: The local file system sync must be ran from the backup
# NOTE: user's crontab not from the cluster resource crons.
#
# ----------------------------------------------------------- 

# Get the user's ENV
if [ -f ~/.bashrc ]; then . ~/.bashrc; fi

export USERNAME=$(whoami)
export DATETIME=$(date -u)
export HOSTNAME=$(hostname -s)

# Check to see if caller has a cluster config file set in their ENV
if [ -z "${CFGFILE}" ]; then export CFGFILE=/etc/drcm/my_cluster_conf/my_cluster_info.sh; fi

if [ ! -f ${CFGFILE} ]; then
    echo "ERROR - Missing ${CFGFILE}"
    exit 1;
fi

# Source the include config 
. ${CFGFILE}

if [ -z "${CLUSTER_NAME}" ]; then
    echo "ERROR - CLUSTER_NAME not set in ${CFGFILE}"
    echo "ERROR - You must set a name for for this cluster"
    exit 1
fi

if [ -z "${PRIMARY_FILESYSTEMS_IP}" ]; then
    echo "ERROR - PRIMARY_FILESYSTEMS_IP not set in ${CFGFILE}"
    echo "ERROR - You must set the IP address for the node with the primary file systems to mirror"
    exit 1
fi

if [ -z "${PRIMARY_FILESYSTEMS_ETH}" ]; then
    echo "ERROR - PRIMARY_FILESYSTEMS_ETH not set in ${CFGFILE}"
    echo "ERROR - You must set the Ethernet interface for the node with the primary file systems to mirror"
    exit 1
fi

if [ -z "${BACKUP_FILESYSTEMS_IP}" ]; then
    echo "ERROR - BACKUP_FILESYSTEMS_IP not set in ${CFGFILE}"
    echo "ERROR - You must set the IP address for the node to backup the primary file systems too"
    exit 1
fi

if [ -z "${BACKUP_FILESYSTEMS_ETH}" ]; then
    echo "ERROR - BACKUP_FILESYSTEMS_ETH not set in ${CFGFILE}"
    echo "ERROR - You must set the Ethernet interface for the node to backup the primary file systems too"
    exit 1
fi

if [ -z "${BACKUP_USER}" ]; then
    echo "ERROR - BACKUP_USER not set in ${CFGFILE}"
    echo "ERROR - You must set the backup user name"
    exit 1
fi

# Allow the user name backup up from one cluster node to another
# to be different than the backup user.
if [ ! -z "${CLUSTER_BACKUP_USER}" ]; then
    echo "INFO - CLUSTER_BACKUP_USER set, will override BACKUP_USER"
    export BACKUP_USER="${CLUSTER_BACKUP_USER}"
fi

if [ "${USERNAME}" != "${BACKUP_USER}" ]; then
    echo "ERROR - You must be ${BACKUP_USER} to run this backup"
    exit 1
fi

# Setup runtime ENV and check for VAR overrides
if [ -z "${LOGdir}" ]; then export LOGdir="/var/log/drcm"; fi
if [ -z "${DRCMdir}" ]; then export DRCMdir="/etc/drcm"; fi
if [ -z "${VARdir}" ]; then export VARdir="/var/run/drcm"; fi
if [ -z "${RYSNC}" ]; then export RYSNC="rsync"; fi
if [ -z "${SSH}" ]; then export SSH="ssh"; fi
if [ -z "${SCP}" ]; then export SCP="scp"; fi
if [ -z "${IP}" ]; then export IP="ip"; fi
if [ -z "${EXPR}" ]; then export EXPR="expr"; fi
if [ -z "${DRCM_server}" ]; then export DRCM_server="/usr/sbin/drcm_server"; fi

# Set RSYNC args for directory mirror
if [ -z "${RYSNCargs}" ]; then export RYSNCargs="-AavPh --delete --sparse --progress --force --stats"; fi

# Set RSYNC args for directory copy
if [ -z "${RYSNCcopyargs}" ]; then export RYSNCcopyargs="-AavPh --sparse --progress --force --stats"; fi

if [ ! -d ${LOGdir} ]; then mkdir -p ${LOGdir}; fi
if [ ! -d ${VARdir} ]; then mkdir -p ${VARdir}; fi

if [ ! -d ${LOGdir} ]; then 
    echo "ERROR - Cannot access ${LOGdir}"
    exit 1
fi

if [ ! -d ${VARdir} ]; then 
    echo "ERROR - Cannot access ${VARdir}"
    exit 1
fi

if [ -z "${SERVICENAME}" ]; then export SERVICENAME="file_system"; fi 

# Set the log file name
LOGFileName="${HOSTNAME}-${SERVICENAME}-filesystem-sync"
LOGfile=$LOGdir/"$LOGFileName.log"

FAILOVERNODE="${BACKUP_FILESYSTEMS_IP}"
FAILBACKNODE="${PRIMARY_FILESYSTEMS_IP}"

# Allow RSYNC on a diffent IP/Interface if high bandwidth required to sync file systems
if [ ! -z $BACKUP_FILESYSTEMS_RSYNC_IP ]; then FAILOVERNODE="$BACKUP_FILESYSTEMS_RSYNC_IP"; fi
if [ ! -z $PRIMARY_FILESYSTEMS_RSYNC_IP ]; then FAILBACKNODE="$PRIMARY_FILESYSTEMS_RSYNC_IP"; fi

# We will default to the failover node for normal operations
BACKUPNODE="${FAILOVERNODE}"

# Command line argument for testing only
if [ "$1" == "failback" ]; then BACKUPNODE="${FAILBACKNODE}"; fi

# Auto detect which node we are on
isprimary=$(${DRCMdir}/utils/is_primary_fs.sh)
isbackup=$(${DRCMdir}/utils/is_backup_fs.sh)

# See if we have failed over
hasfailedover=$(${DRCMdir}/utils/fs_has_failedover.sh)

if [ -z ${hasfailedover} ]; then
    echo "ERROR - My cluster info setup error, missing VARs"
    exit 1
fi

if [ "${isprimary}" == "TRUE" ];  then BACKUPNODE="${FAILOVERNODE}"; fi
if [ "${hasfailedover}" == "TRUE" ];  then BACKUPNODE="${FAILBACKNODE}"; fi
if [ "${isbackup}" == "TRUE" ];  then BACKUPNODE="${FAILBACKNODE}"; fi

# Source include scripts
. ${DRCMdir}/utils/calc_runtime.sh
. ${DRCMdir}/utils/process_lock.sh

PROGRAMname="$0"
LOCKfile="${VARdir}/${SERVICENAME}_file_system_sync.lck"

# Set the max lock time for this process
if [ -z "${MINold}" ]; then export MINold="45"; fi

error_level=0

function RsyncMirror()
{
    if [ -z ${1} ]
	then
	echo "Bad call to RsyncMirror function" | tee -a ${LOGfile}
	echo "Usage: RsyncMirror /SourceDirectory" | tee -a ${LOGfile}
	error_level=1
	return
    fi

    ${SSH} -o stricthostkeychecking=no -n ${BACKUPNODE} mkdir -p ${1}  >> ${LOGfile} 2>&1
    if [ $? -ne 0 ]
	then
	echo "SSH command to ${BACKUPNODE} failed" | tee -a ${LOGfile}
	echo "${1} mirrored backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi

    echo "Syncing ${1} with rsync mirror"  | tee -a ${LOGfile}
    ${RYSNC} ${RYSNCargs} -e ${SSH} ${1}/. ${BACKUPNODE}:${1}/. >> ${LOGfile} 2>&1
    if [ $? -ne 0 ]
	then
	echo "${1} mirrored backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi
    echo "${1} mirrored backup complete" | tee -a ${LOGfile}
}

function RsyncCopy()
{
    if [ -z ${1} ]
	then
	echo "Bad call to RsyncCopy function" | tee -a ${LOGfile}
	echo "Usage: RsyncCopy /SourceDirectory" | tee -a ${LOGfile}
	error_level=1
	return
    fi

    ${SSH} -o stricthostkeychecking=no -n ${BACKUPNODE} mkdir -p ${1}  >> ${LOGfile} 2>&1
    if [ $? -ne 0 ]
	then
	echo "SSH command to ${BACKUPNODE} failed" | tee -a ${LOGfile}
	echo "${1} backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up" | tee -a ${LOGfile}
	error_level=1
	return
    fi

    echo "Syncing up ${1} with rsync copy" | tee -a ${LOGfile}
    ${RYSNC} ${RYSNCcopyargs} ${SSH} ${1}/. ${BACKUPNODE}:${1}/. >> ${LOGfile} 2>&1
    if [ $? -ne 0 ]
	then
	echo "${1} backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up" | tee -a ${LOGfile}
	error_level=1
	return
    fi
    echo "${1} backup complete" | tee -a  ${LOGfile}
}

echo "Logging output to ${LOGfile}"

LockFileCheck $MINold
CreateLockFile

# Clear the log file
cat /dev/null > ${LOGfile}

echo "${HOSTNAME} file system sync log" | tee -a ${LOGfile}
if [  "${hasfailedover}" == "ERROR" ]; then
    echo "ERROR - Both cluster nodes are down" | tee -a ${LOGfile}
    echo "ERROR - Exiting with no file systems sync'ed" | tee -a ${LOGfile}
    exit 1
fi

if [ "${hasfailedover}" == "FALSE" ]
    then
    if [ -e ${VARdir}/${SERVICENAME}_failback_sync_flag ]
	then 
	MAXHOURS="1"
	lastran=$(cat ${VARdir}/${SERVICENAME}_failback_sync_flag)
	currtime=$(date +%s)
	diff=$((currtime - lastran))
	HRS=$(${EXPR} $diff / 3600)
	MIN=$(${EXPR} $diff % 3600 / 60)
	SEC=$(${EXPR} $diff % 3600 % 60)

	echo "We have have a failback sync flag" | tee -a ${LOGfile}
	echo -n "Last failback flag from secondary node received: " | tee -a ${LOGfile}
	if [ $HRS -gt 0 ]; then echo -n "$HRS hours "; fi
	if [ $MIN -gt 0 ]; then echo -n "$MIN minutes "; fi
	if [ $SEC -gt 0 ]
	    then
	    if [ $MIN -gt 0 ]
		then
		echo -n "and $SEC seconds "
	    elif [ $HRS -gt 0 ]
		then
		echo -n "and $SEC  seconds "
	    else
		echo -n "$SEC seconds "
	    fi
	fi
	echo " ago."
	echo -n "Date of last failback flag: " | tee -a ${LOGfile}
	echo ${lastran} | awk '{ print strftime("%m/%d/%Y %H:%m:%S", $1) }' | tee -a ${LOGfile}

	if [ "${isprimary}" == "TRUE" ]
	    then
	    if [ $HRS -ge $MAXHOURS ]
		then
		echo "Our fail over was more then ${HRS} ago." | tee -a ${LOGfile}
		echo "Ingnoring the flag and continuing primary backup" | tee -a ${LOGfile}
		BACKUPNODE="${FAILOVERNODE}"
	    else
		echo "Skipping file sync until secondary node updates primary file system"
		RemoveLockFile
		exit 0
	    fi
	fi

	if [ "${isprimary}" == "FALSE" ]
	    then
	    echo "DRCM cluster has failed back to primary node" | tee -a ${LOGfile}
	    if [ $HRS -ge $MAXHOURS ]
		then
		echo "Our fail over was more then ${HRS} ago." | tee -a ${LOGfile}
		echo "Removing all flagback flags and exiting" | tee -a ${LOGfile}
		rm -f ${VARdir}/${SERVICENAME}_failback_sync_flag
		${SSH} -o stricthostkeychecking=no "${FAILBACKNODE}" "rm -f ${VARdir}/${SERVICENAME}_failback_sync_flag" 2>&1
		RemoveLockFile
		exit 0
	    else
		BACKUPNODE="${FAILBACKNODE}"
	    fi
	fi
    fi
fi

if [ "${hasfailedover}" == "TRUE" ]
    then
    echo "DRCM cluster has failed over to ${FAILOVERNODE}" | tee -a ${LOGfile}

    if [ "${isprimary}" == "TRUE" ]
	then 
	echo "This is the master cluster node is up" | tee -a ${LOGfile}
	echo "No operation will be preformed" | tee -a ${LOGfile}
	RemoveLockFile
	exit 0
    fi

    echo "Signaling that a failback sync is needed when normal cluster operation is resumed"  | tee -a ${LOGfile}
    date +%s > ${VARdir}/${SERVICENAME}_failback_sync_flag
    ping -c 15 ${FAILBACKNODE} > /dev/null 2>&1
    if [ $? -ne 0 ]
	then
	echo "Primary cluster node is still down" | tee -a ${LOGfile}
	echo "Cannot write failback sync flag to ${FAILBACKNODE}" | tee -a ${LOGfile}
    else
	echo "Primary cluster node is up" | tee -a ${LOGfile}
	echo "Writing failback sync flag to ${FAILBACKNODE}" | tee -a ${LOGfile}
	${SSH} -o stricthostkeychecking=no ${FAILBACKNODE} mkdir -p ${VARdir} >> ${LOGfile} 2>&1
	${SCP} -o stricthostkeychecking=no ${VARdir}/${SERVICENAME}_failback_sync_flag ${FAILBACKNODE}:${VARdir}/${SERVICENAME}_failback_sync_flag
    fi
    RemoveLockFile
    exit 0
fi

if [ "${BACKUPNODE}" == "${FAILBACKNODE}" ]
    then
    echo "DRCM cluster requested failback file system sync from secondary cluster node to primary" | tee -a ${LOGfile}
    echo "Checking to see if failback sync flag exists" | tee -a ${LOGfile}
    if [ ! -e ${VARdir}/${SERVICENAME}_failback_sync_flag ]
	then 
	echo "We have no sync flag for ${SERVICENAME} files" | tee -a ${LOGfile}
	echo "No file sync to primary cluster node will be performed" | tee -a ${LOGfile}
	RemoveLockFile
	exit 1
    fi

    echo "We are going back to the primary cluster node ${FAILBACKNODE}" | tee -a ${LOGfile}
    echo "Checking to see if primary cluster node is up and acceping pings" | tee -a ${LOGfile}
    ping -c 15 ${FAILBACKNODE} > /dev/null 2>&1
    if [ $? -ne 0 ]
	then
	echo "Primary cluster node is still down" | tee -a ${LOGfile}
	echo "No file sync to primary cluster node will be performed" | tee -a ${LOGfile}
	RemoveLockFile
	exit 1
    fi
else
    echo "Normal cluster operations, syncing to failover node ${FAILOVERNODE}" | tee -a ${LOGfile}
fi
echo "Starting file sync at $datetime UTC" | tee -a ${LOGfile}

# Start of backups
# ----------------------------------------------------------- 
datetime=$(date -u)
echo "File system sync to ${BACKUPNODE}..."  | tee -a ${LOGfile}
echo "Starting file system sync sync $datetime UTC" | tee -a ${LOGfile}
START=$(date +%s)

# Source the config file for this host
if [ -e ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh ]
    then
    . ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    # Check for host specific override config
    if [ -f ${DRCMdir}/my_cluster_conf/$(hostname -s)_${SERVICENAME}_sync_list.sh ]
    then
	. ${DRCMdir}/my_cluster_conf/$(hostname -s)_${SERVICENAME}_sync_list.sh
    fi
else
    echo "WARNING - No file system sync preformed" | tee -a ${LOGfile}
    echo "Missing ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh, will create default and exit" | tee -a ${LOGfile}
    touch ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "# ${SERVICENAME} file sync to DRCM cluster node $BACKUPNODE" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo -n "# Last modified: " >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    date >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "# TODO: Add or remove directories to mirror here" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "#RsyncMirror ""/home""" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "# TODO: Add or remove directories to copy here" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "#RsyncCopy ""/usr/local""" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
    echo "" >> ${DRCMdir}/my_cluster_conf/${SERVICENAME}_sync_list.sh
fi
# ----------------------------------------------------------- 
FINISH=$(date +%s)

datetime=$(date -u)
echo "Ending file sync at $datetime UTC" | tee -a ${LOGfile}

calc_runtime $START $FINISH | tee -a ${LOGfile}

echo "File system sync complete"  | tee -a ${LOGfile}
echo "Exiting script..."  | tee -a ${LOGfile}

# Remove the failback sync flags
if [ "${BACKUPNODE}" == "${FAILBACKNODE}" ]
    then
    echo "Removing all flagback flags and exiting" | tee -a ${LOGfile}
    rm -f ${VARdir}/${SERVICENAME}_failback_sync_flag
    ${SSH} -o stricthostkeychecking=no "${FAILBACKNODE}" rm -f ${VARdir}/${SERVICENAME}_failback_sync_flag 2>&1
fi

RemoveLockFile
exit 0
# ----------------------------------------------------------- 
# ******************************* 
# ********* End of File ********* 
# ******************************* 
