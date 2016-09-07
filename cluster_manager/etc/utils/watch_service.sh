#!/bin/bash
# Script ran from root's crontab to ensure a system service is running
# */2 * * * * /etc/drcm/utils/watch_service.sh drlb_server > /var/log/drcm/watch_service.sh_cron.log 2>&1

if [ "${1}" == "" ] 
then
    echo "ERROR - You must provide a service name"
    exit 1
fi

service="${1}"

source /etc/drcm/utils/process_lock.sh

PROGRAMname="$0"
LOCKfile="/var/run/drcm/watch_service_${$}.lck"
MINold="5"

LockFileCheck $MINold
CreateLockFile

echo "Watching ${service}"

/etc/drcm/resources/service.sh ${service} watch

RemoveLockFile
