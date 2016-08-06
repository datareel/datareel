#!/bin/bash
# ----------------------------------------------------------- 
# BASH Script
# Operating System(s): RHEL/CENTOS
# Run Level(s): 3, 5
# Shell: BASH shell
# Original Author(s): DataReel Software Development
# File Creation Date: 07/17/2016 
# Date Last Modified: 08/05/2016
#
# Version control: 1.05
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
# Complex locking protocol functions for BASH scripts
#
# This script is designed to be sourced from other scripts.
# Before calling the lock file functions the caller must 
# define the $LOCKfile variable that defined the lock file
# used by the caller to lock a process. 
# ----------------------------------------------------------- 

function CreateLockFile()
{
    # Build a new lock file using "hostname:PID" format
    hostname=`hostname`
    echo -n "${hostname}:" > ${LOCKfile}
    echo -n $$ >> ${LOCKfile}
    chmod 777 ${LOCKfile}
}

function RemoveLockFile()
{
    rm -f ${LOCKfile}
}

function SimpleLockFileCheck()
{
    # Simple check and remove for lock file

    # Set the function variables
    MINold="60"
    if [ "$1" != "" ] 
	then 
	MINold="$1"
    fi

    echo "Checking for lock file older than $MINold minutes"
    find ${LOCKfile} -mmin +$MINold -type f -print -exec rm -f {} \; &> /dev/null 
    if [ -e ${LOCKfile} ]
	then
	echo "Lock file still exists: ${LOCKfile}"
	echo "Application may still be running or has terminated before completion"
	echo "Exiting..."
	exit 1
    fi
    echo "Removing the old lock file and continuing to run"
}

function LockFileCheck()
{
    # Complex locking protocol

    # Set the function variables
    MINold="60"
    if [ "$1" != "" ] 
	then 
	MINold="$1"
    fi

    if [ -e ${LOCKfile} ]
	then
	echo "A lock file exists: ${LOCKfile}"
    
        # Read the hostname and the PID from the lock file
	LOCKhostname=`awk -F: '{ print $1 }' ${LOCKfile}`
	LOCKpid=`awk -F: '{ print $2 }' ${LOCKfile}`

	# Check our current hostname
	hostname=`hostname`
	if [ "$hostname" != "$LOCKhostname" ]
	    then
	    echo "Process is locked on another node $LOCKhostname"
	    echo "Checking the lock file age, max is $MINold minutes"
	    SimpleLockFileCheck $MINold
	    return 
	fi
	echo "Checking to see how long the process has been running"
	currlockfile=`find ${LOCKfile} -mmin +$MINold -type f -print`
	if [ "${currlockfile}" == "${LOCKfile}" ]
	    then
	    echo "PID ${LOCKpid} has been running for more than $MINold minutes"
	    ispidvalid=`ps -e | grep ${LOCKpid} | grep -v grep | awk '{print $1}'`
	    if [ "$ispidvalid" == "$LOCKpid" ] 
		then
		echo "Attempting to kill the previous process ${LOCKpid} and any children"
		pidlist=`ps -ef | grep $LOCKpid | grep -v grep | awk '{print $2}' | sort -rn`
		for pids in $pidlist
		do
		  echo "Killing PID $pids"
		  kill $pids
		  ispidvalid=`ps -ef | grep ${pids} | grep -v grep | awk '{print $2}'`
		  if [ "$ispidvalid" == "$pids" ] 
		      then
		      echo "Could not kill process $pids for process ${LOCKpid}" 
		      echo "${LOCKpid}  process is still running" 
		      echo "Application may be running or has terminated before completion"
		      echo "Exiting..."
		      exit 1
		  fi
		done

                # Check for PIDs that respawned after killing a child 
            	pidlist=`ps -ef | grep $LOCKpid | grep -v grep | awk '{print $2}' | sort -rn`
		for pids in $pidlist
		do
		  echo "Killing respawned PID $pids"
		  ispidvalid=`ps -e | grep $pids | grep -v grep | awk '{print $1}'`
		  # Make sure the PID is still active are killing child process
		  if [ "$ispidvalid" == "$pids" ] 
		      then
		      kill $pids
		  fi
		  # Check to see if the PID is still running
		  ispidvalid=`ps -ef | grep ${pids} | grep -v grep | awk '{print $2}'`
		  if [ "$ispidvalid" == "$pids" ] 
		      then
		      echo "Could not kill process $pids for process ${LOCKpid}" 
		      echo "${LOCKpid}  process is still running" 
		      echo "Application may be running or has terminated before completion"
		      echo "Exiting..."
		      exit 1
		  fi
		done
	    else
		echo "PID ${LOCKpid} is not running"
		echo "Removing the old lock file and continuing to run"
		rm -f ${LOCKfile}
		return
	    fi
	fi
	
	# Check to see if the process is still running
	ispidvalid=`ps -e | grep ${LOCKpid} | grep -v grep | awk '{print $1}'`
	echo "Checking to see if the previous process is still running"
	if [ "$ispidvalid" == "$LOCKpid" ] 
	    then
	    echo "${LOCKpid}  process is still running" 
	    echo "Application may be running or has terminated before completion"
	    echo "Exiting..."
	    exit 1
	fi

	echo "Previous process is not running"
	echo "Removing the old lock file and continuing to run"
	rm -f ${LOCKfile}
    fi
} 
# ----------------------------------------------------------- 
# ******************************* 
# ********* End of File ********* 
# ******************************* 


