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
# Version control: 1.06
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
# Include script used to calculate a process script's run time.
#
# -----------------------------------------------------------

function calc_runtime() {
    # Caller must supply a start and finish time by adding the following
    # lines at the top and bottom of the script or function:
    #
    # START=`date +%s`
    # ...
    # FINISH=`date +%s`
    #
    # source ~/bin/calc_runtime.sh
    # calc_runtime $START $FINISH "My process"
    #

    START="$1"
    FINISH="$2"
    PROCNAME="${3}"

    diff=$((FINISH - START))
    if [ $diff -le 0 ]; then diff=1; fi
    echo -n "Total run time for ${PROCNAME}: "
    HRS=`expr $diff / 3600`
    MIN=`expr $diff % 3600 / 60`
    SEC=`expr $diff % 3600 % 60`
    if [ $HRS -gt 0 ]
	then
	echo -n "$HRS hrs. "
    fi
    if [ $MIN -gt 0 ]
	then
	echo -n "$MIN mins. "
    fi
    if [ $SEC -gt 0 ]
	then
	if [ $MIN -gt 0 ]
	    then
	    echo "and $SEC secs."
	elif [ $HRS -gt 0 ]
	    then
	    echo "and $SEC secs."
	else
	    echo "$SEC secs."
	fi
    fi
}

# ----------------------------------------------------------- 
# ******************************* 
# ********* End of File ********* 
# ******************************* 
