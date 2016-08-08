#!/bin/bash
# BASH Script
# Operating System(s): RHEL/CENTOS
# Run Level(s): 3, 5
# Shell: BASH shell
# Original Author(s): DataReel Software Development
# File Creation Date: 07/17/2016 
# Date Last Modified: 08/08/2016
#
# Version control: 1.12
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
# DRCM cluster script used to backup user and other shared directories 
# to a shared storage device. This script should be ran from a cron
# resource script on a primary cluster node. In the event of a failover
# the resource cron will run this script on the back node where the 
# resrouce cron failed over too.
#
# NOTE: This script requires a designated backup user to have 
# NOTE: SSH public key authentication to the server hosting 
# NOTE: the backup storage directory. The resource cron must
# NOTE: be ran as the backup user.
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

if [ -z "${BACKUP_STORAGE_SERVER}" ]; then
    echo "ERROR - BACKUP_STORAGE_SERVER not set in ${CFGFILE}"
    echo "ERROR - You must set an IP or hostname of the storage server"
    exit 1
fi

if [ -z "${BACKUP_STORAGE_LOCATION}" ]; then
    echo "ERROR - BACKUP_STORAGE_LOCATION not set in ${CFGFILE}"
    echo "ERROR - You must set the destination directory on the storage server"
    exit 1
fi

if [ -z "${BACKUP_USER}" ]; then
    echo "ERROR - BACKUP_USER not set in ${CFGFILE}"
    echo "ERROR - You must set the backup user name"
    exit 1
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

# Set the log file name
LOGFileName="${CLUSTER_NAME}-shared-storage-backup"
LOGfile=$LOGdir/"$LOGFileName.log"

# Source include scripts
. ${DRCMdir}/utils/calc_runtime.sh
. ${DRCMdir}/utils/process_lock.sh

PROGRAMname="$0"
LOCKfile="${VARdir}/backup_to_shared_storage.lck"

# Set the max lock time for this process
if [ -z "${MINold}" ]; then export MINold="45"; fi

error_level=0

# Script functions
# ----------------------------------------------------------- 
RsyncMirror()
{
    if [ -z "${1}" ]
	then
	echo "Bad call to RsyncMirror function"
	echo "Usage: RsyncMirror /SourceDirectory"
	error_level=1
	return
    fi

    ${SSH} -o stricthostkeychecking=no -n ${BACKUP_STORAGE_SERVER} mkdir -p ${BACKUP_STORAGE_LOCATION}${1}  >> ${LOGfile}
    if [ $? -ne 0 ]
	then
	echo "SSH command to ${BACKUP_STORAGE_SERVER} failed" | tee -a ${LOGfile}
	echo "${1} mirrored backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi

    echo "Backing up ${1} with rsync mirror"  | tee -a ${LOGfile}
    ${RYSNC} ${RYSNCargs} -e ${SSH} ${1}/. ${BACKUP_STORAGE_SERVER}:${BACKUP_STORAGE_LOCATION}${1}/. >> ${LOGfile} 2>&1
    if [ $? -ne 0 ]
	then
	echo "${1} mirrored backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi
    echo "${1} mirrored backup complete"  | tee -a ${LOGfile}
}

RsyncCopy()
{
    if [ -z "${1}" ]
	then
	echo "Bad call to RsyncCopy function"
	echo "Usage: RsyncCopy /SourceDirectory"
	error_level=1
	return
    fi

    ${SSH} -o stricthostkeychecking=no -n ${BACKUP_STORAGE_SERVER} mkdir -p ${BACKUP_STORAGE_LOCATION}${1}  >> ${LOGfile}
    if [ $? -ne 0 ]
	then
	echo "SSH command to ${BACKUP_STORAGE_SERVER} failed" | tee -a ${LOGfile}
	echo "${1} backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi

    echo "Backing up ${1} with rsync copy"  | tee -a ${LOGfile}
    ${RYSNC} ${RYSNCcopyargs} -e ${SSH} ${1}/. ${BACKUP_STORAGE_SERVER}:${BACKUP_STORAGE_LOCATION}${1}/. >> ${LOGfile} 2>&1
    if [ $? -ne 0 ]
	then
	echo "${1} backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi
    echo "${1} backup complete"  | tee -a ${LOGfile}
}

RsyncRotate()
{
    if [ -z "${1}" ]
	then
	echo "Bad call to RsyncRotate function"
	echo "Usage1: RsyncRotate /SourceDirectory"
	echo "Usage2: RsyncRotate /SourceDirectory 30"
	return
    fi

    # Set the number of backups to 7 unless the call requests 30 days
    BackupNumber=$(date -u +%w)
    if [ ! -z ${2} ] && [ ${2} -eq 30 ]
	then
	BackupNumber=$(date -u +%d)
	echo "Preforming 30 day rotating backup" | tee -a ${LOGfile}
    fi

    ${SSH} -o stricthostkeychecking=no -n ${BACKUP_STORAGE_SERVER} mkdir -p ${BACKUP_STORAGE_LOCATION}/Rotating/${BackupNumber}${1}  >> ${LOGfile}
    if [ $? -ne 0 ]
	then
	echo "SSH command to ${BACKUP_STORAGE_SERVER} failed" | tee -a ${LOGfile}
	echo "${1} rotating mirrored backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi

    echo "Backing up ${1} with rsync rotating mirror"  | tee -a ${LOGfile}
    ${RYSNC} ${RYSNCargs} -e ${SSH} ${1}/. ${BACKUP_STORAGE_SERVER}:${BACKUP_STORAGE_LOCATION}/Rotating/${BackupNumber}${1}/. >> ${LOGfile} 2>&1
    if [ $? -ne 0 ]
	then
	echo "${1} rotating mirrored backup failed" | tee -a ${LOGfile}
	echo "${1} was not backed up"  | tee -a ${LOGfile}
	error_level=1
	return
    fi
    echo "${1} rotating mirrored backup complete"  | tee -a ${LOGfile}
}

echo "INFO - Log file: ${LOGfile}"

LockFileCheck ${MINold}
CreateLockFile

# Clear the log file
cat /dev/null > ${LOGfile}

echo "${HOSTNAME} shared stropage backup log" | tee -a ${LOGfile}
echo "Starting backup at ${DATETIME} UTC" | tee -a ${LOGfile}
echo "Backup server ${BACKUP_STORAGE_SERVER}" | tee -a ${LOGfile}
echo "Backup server directory${BACKUP_STORAGE_LOCATION}" | tee -a ${LOGfile}

${SSH} -o stricthostkeychecking=no -n ${BACKUP_STORAGE_SERVER} mkdir -p ${BACKUP_STORAGE_LOCATION}  >> ${LOGfile}
if [ $? -ne 0 ]
    then
    echo "SSH command to ${BACKUP_STORAGE_SERVER} failed" | tee -a ${LOGfile}
    echo "Backup script cannot continue"  | tee -a ${LOGfile}
    echo "Exiting script with errors..."  | tee -a ${LOGfile}
    RemoveLockFile
    exit 1
fi

# Start of backups
# ----------------------------------------------------------- 
echo "Backing up directories to share storage on ${BACKUP_STORAGE_SERVER}"  | tee -a ${LOGfile}

echo "Starting backup ${DATETIME} UTC" | tee -a ${LOGfile}
START=$(date +%s)

# Source the config file for this cluster node
if [ -e ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh ]; then
    . ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh

    # Check for host specific override config
    if [ -f ${DRCMdir}/my_cluster_conf/$(hostname -s)_${CLUSTER_NAME}_shared_storage_sync_list.sh ]; then
	. ${DRCMdir}/my_cluster_conf/$(hostname -s)_${CLUSTER_NAME}_shared_storage_sync_list.sh
    fi
else
    echo "WARNING - No file system sync preformed" | tee -a ${LOGfile}
    echo "WARNING - Missing ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh" | tee -a ${LOGfile}
    echo "WARNING - Will create default list and exit" | tee -a ${LOGfile}
    touch ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "# Backup list include file for ${HOSTNAME}" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo -n "# Last modified: " >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    date >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "# TODO: Add or remove directories to mirror here" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "##RsyncMirror ""/home""" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "##RsyncMirror ""/usr/local"""  >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "##RsyncMirror ""/var/spool/cron""" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "##RsyncMirror ""/var/www""" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "# TODO: Add or remove directories to copy here" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "##RsyncCopy ""/boot""" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "# TODO: Add or remove rotating backups here" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    echo "##RsyncRotate ""/var/lib/mysql""" >> ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
    . ${DRCMdir}/my_cluster_conf/${CLUSTER_NAME}_shared_storage_sync_list.sh
fi
# ----------------------------------------------------------- 
FINISH=$(date +%s)

DATETIME=$(date -u)
echo "Ending network backup at ${DATETIME} UTC" | tee -a ${LOGfile}

if [ $FINISH -gt $START ]; then
    calc_runtime $START $FINISH | tee -a ${LOGfile}
fi

if [ $error_level -ne 0 ]; then
    echo "ERROR - Shared storage backup failed" | tee -a ${LOGfile}
else
    echo "Shared storage backup complete" | tee -a ${LOGfile}
fi

echo "Exiting shared storage backup script..." | tee -a ${LOGfile}

RemoveLockFile
exit ${error_level}
# ----------------------------------------------------------- 
# ******************************* 
# ********* End of File ********* 
# ******************************* 
