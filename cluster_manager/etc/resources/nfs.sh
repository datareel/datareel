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
# Version control: 1.14
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
# DR cluster resource script for watching cluster NFS mounts
#
# All NFS resource scripts must use the following args:
#
# ${1} = Source: "hostname:dir"
# ${2} = Traget: "dir"
# ${3} = Actions: mount|unmount|remount|watch
#
# ----------------------------------------------------------- 

. ~/.bashrc

error_level=0

if [ -z ${1} ]; then
    echo "ERROR - You must specify a source"
    error_level=1;
fi

if [ -z ${2} ]; then
    echo "ERROR - You must specify a target"
    error_level=1;
fi

if [ -z ${3} ]; then
    echo "ERROR - You must specify an action"
    error_level=1;
fi

if [ $error_level -ne 0 ]; then
    echo "Usage: ${0} hostname:source_dir target_dir mount|unmount|remount|watch"
    exit $error_level
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
	# Tesing network connection to ${nfshost}
	ping -c 3 ${nfshost} > /dev/null 2>&1
	if [ $? -ne 0 ]; then
	    echo "ERROR - Bad network connection to ${nfshost}"
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
	mount ${source} ${target}
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
	mount -o remount ${target}
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

is_mounted=0;
source="${1}"
nfshost=$(echo "${1}" | awk -F':' '{ print $1 }')
target="${2}"
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
