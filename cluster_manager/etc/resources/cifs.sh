#!/bin/bash
# ----------------------------------------------------------- 
# BASH Script
# Operating System(s): RHEL/CENTOS
# Run Level(s): 3, 5
# Shell: BASH shell
# Original Author(s): DataReel Software Development
# File Creation Date: 07/17/2016 
# Date Last Modified: 07/27/2016
#
# Version control: 1.15
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
# DR cluster resource script for watching cluster DFS mounts
#
# All CIFS resource scripts must use the following args:
#
# ${1} = Source UNC path: "//hostname/path"
# ${2} = Traget: "dir"
# ${3} = Actions: mount|unmount|remount|watch
#
# DFS Windows shares must have an Active Directory (AD) serivce account.
# SAMBA shares must have a local Windows user account or AD account.
#
# This script requires an auth file in the default DRCM auth DIR.
# A default auth file will be generated the first time this script is
# executed. You will need to edit the auth file for Windows 
# service account or user account.
# ----------------------------------------------------------- 

. ~/.bashrc
error_level=0

if [ -z ${1} ]; then
    echo "ERROR - You must specify a source UNC path: example //hostname/path/shared_dir"
    error_level=1;
fi

if [ -z ${2} ]; then
    echo "ERROR - You must specify a target: example /mnt/DFS"
    error_level=1;
fi

if [ -z ${3} ]; then
    echo "ERROR - You must specify an action"
    error_level=1;
fi

if [ $error_level -ne 0 ]; then
    echo "Usage: ${0} //hostname//source_dir target_dir mount|unmount|remount|watch"
    exit $error_level
fi

is_mounted=0;
source="${1}"
dfshost=$(echo "${1}" | sed s,//,,g | awk -F'/' '{ print $1 }')
target="${2}"

if [ ! -d /etc/drcm/.auth ]; then
    mkdir -p /etc/drcm/.auth
    chmod 700 /etc/drcm/.auth
fi

if [ ! -f /etc/drcm/.auth/${dfshost} ]; then
    error_level=1
    echo "ERROR - Missing /etc/drcm/.auth/${dfshost} auth file"
    echo "ERROR - Creating default CIFS auth file and exiting"
    cat /dev/null > /etc/drcm/.auth/${dfshost}
    echo "# DFS auth file for resource script" >> /etc/drcm/.auth/${dfshost}
    echo "# Last modified: $(date)" >> /etc/drcm/.auth/${dfshost}
    echo "" >> /etc/drcm/.auth/${dfshost}
    echo "# You must the values below for this DFS mount" >> /etc/drcm/.auth/${dfshost}
    echo "" >> /etc/drcm/.auth/${dfshost}
    echo 'export username=""' >> /etc/drcm/.auth/${dfshost}
    echo "export password=''" >> /etc/drcm/.auth/${dfshost}
    echo "export passwordage=60" >> /etc/drcm/.auth/${dfshost}
    echo 'export dir_mode="0775"' >> /etc/drcm/.auth/${dfshost}
    echo 'export file_mode="0775"' >> /etc/drcm/.auth/${dfshost}
    echo 'export uid=0' >> /etc/drcm/.auth/${dfshost}
    echo 'export gid=100' >> /etc/drcm/.auth/${dfshost}
    echo "" >> /etc/drcm/.auth/${dfshost}
    echo "# NOTE: After each password change update the .last file:" >> /etc/drcm/.auth/${dfshost}
    echo "# date +%s > /var/run/drcm/${dfshost}.last" >> /etc/drcm/.auth/${dfshost}
    echo "" >> /etc/drcm/.auth/${dfshost}
    chmod 600 /etc/drcm/.auth/${dfshost}
    if [ ! -d /var/run/drcm ]; then mkdir -p /var/run/drcm; fi
    date +%s > /var/run/drcm/${dfshost}.last
    chmod 664 /var/run/drcm/${dfshost}.last
    exit $error_level
fi

. /etc/drcm/.auth/${dfshost}

# Check the age of the Windows user account password for this share.
# If the Windows password is expired create a .expired file in the CM run_dir
if [ ! -z ${passwordage} ]; then
    if [ ! -d /var/run/drcm ]; then mkdir -p /var/run/drcm; fi
    if [ ! -f /var/run/drcm/${dfshost}.last ]; then date +%s > /var/run/drcm/${dfshost}.last; fi
    last_password_change=$(cat /var/run/drcm/${dfshost}.last)
    etime=$(date +%s)
    expired_etime=$(echo "${last_password_change} + (((60*60)*24)*${passwordage})" | bc)
    if [ -z  ${etime} ] && [ -z ${expired_etime} ]; then
	if [ ${etime} -ge ${expired_etime} ]; then
	    echo "${expired_etime}" > /var/run/drcm/${dfshost}.expired
	else 
	    if [ -f /var/run/drcm/${dfshost}.expired ]; then rm -f /var/run/drcm/${dfshost}.expired; fi
	fi
    fi
fi

if [ -z ${username} ]; then
    echo "ERROR - Windows service account username not set in DFS auth file"
    exit 1
fi

if [ -z ${password} ]; then
    echo "ERROR - Windows service account password not set in DFS auth file"
    exit 1
fi

function IsMounted() {
    is_mounted=0;
    grep "${source} ${target}" /etc/mtab &> /dev/null
    if [ $? -eq 0 ]; then is_mounted=1; fi
}

function Mount() {
    IsMounted
    if [ $is_mounted -eq 0 ]; then
	# Mounting ${source} to ${target}
	error_level=0
	# Tesing network connection to ${dfshost}
	ping -c 3 ${dfshost} > /dev/null 2>&1
	if [ $? -ne 0 ]; then
	    echo "ERROR - Bad network connection to ${dfshost}"
	    echo "ERROR - Cannot mount ${source} to ${target}" 
	    error_level=1
	    return
	fi
	# Attempting to mount ${source} ${target}
	if [ ! -d ${target} ]; then
	    # Target DIR does not exist ${target}
	    mkdir -vp ${target}
	    chmod 777 ${target}
	fi
	cat /dev/null > ${target}/not_mounted
	mount -t cifs ${source} ${target} -o username=${username},password=${password},dir_mode=${dir_mode},file_mode=${file_mode},uid=${uid},gid=${gid}
	IsMounted
	if [ $is_mounted -eq 0 ]; then
	    echo "ERROR - Cannot mount ${source} to ${target}" 
	    error_level=1
	    return
	fi
	# ${source} ${target} mounted sucessfully
    fi
}

function Unmount() {
    error_level=0
    IsMounted
    if [ $is_mounted -ne 0 ]; then
	# Attempting to unmount ${target} from ${source}
	umount -lf ${target}
	IsMounted
	if [ $is_mounted -ne 0 ]; then
	    echo "ERROR - Could not unmount ${target} from ${source}"
	    error_level=1
	    return
	fi
	# unmounted ${target} from ${source}
    fi
}

function Remount() {
    IsMounted
    if [ $is_mounted -eq 0 ]; then
	echo "${target} is not mounted"
	Mount
    else
	mount -t cifs ${source} ${target} -o remount,username=${username},password=${password},dir_mode=${dir_mode},file_mode=${file_mode},uid=${uid},gid=${gid}
	IsMounted
	if [ $is_mounted -eq 0 ]; then
	    echo "ERROR - ${target} unmounted following remount"
	    umount -lf ${target}
	    Mount
	fi
    fi
    IsMounted
    if [ $is_mounted -eq 0 ]; then
	echo "ERROR - Remount failed: ${target} unmounted following remount"
	error_level=1
    fi
}

function Watch()
{
    error_level=0
    IsMounted
    if [ $is_mounted -eq 0 ]; then
	echo "Watch ${target} is not mounted"
	Mount
    fi

    # Checking for stale NFS mounts
    ls -l --color=none ${target} &> /tmp/ls.$$
    grep -i "stale" /tmp/ls.$$ | grep -i "handle" &> /dev/null
    if [ $? -eq 0 ]
    then 
	echo "FAIL - ${source} ${target} is stale"
	echo "Will attempt a remount"
	mount -o remount ${target}
	ls -l ${target} &> /tmp/ls.$$
	grep -i "stale" /tmp/ls.$$ | grep -i "handle" &> /dev/null
	if [ $? -eq 0 ]
	then
	    echo "ERROR - ${source} ${target} is stale after remount"
	    echo "Will attempt a unmount and mount"
	    umount -lf ${target}
	    mount ${source} ${target}
	    IsMounted
	    if [ $is_mounted -eq 0 ]; then
		echo "ERROR - Cannot mount ${source} to ${target}" 
		error_level=1
	    fi
	fi
    fi
    rm -f /tmp/ls.$$
}

case "${3}" in
    mount)
	# Mounting ${source} to ${target}
	Mount
        ;;
    unmount|umount)
	# Unmounting ${target}
	Unmount
        ;;
    remount)
	# Remounting ${source} to ${target}
	Remount
        ;;
    watch)
	Watch
        ;;
    *)
        echo "ERROR - Action not valid ${3}"
	echo "Valid actions are: mount|unmount|remount|watch"
        exit 1
esac

exit $error_level
# ----------------------------------------------------------- 
# ******************************* 
# ********* End of File ********* 
# ******************************* 
