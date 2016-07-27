#!/bin/bash
# DR cluster resource script for adding or removing IPv4 addresses

. ~/.bashrc

# All IPv4 resouce scripts must use the following args:
#
# ${1} = IP address, example: 192.168.122.24
# ${2} = Netmask, examples: 255.255.255.0 or CIDR notation: 24
# ${3} = Ethernet interface, example: eth0:1
# ${4} = actions: start|stop

error_level=0

if [ -z ${1} ]; then
    echo "ERROR - You must specify and IPv4 address"
    error_level=1;
fi

if [ -z ${2} ]; then
    echo "ERROR - You must specify a netmask"
    error_level=1;
fi

if [ -z ${3} ]; then
    echo "ERROR - You must specify an Ehternet interface"
    error_level=1;
fi

if [ -z ${4} ]; then
    echo "ERROR - You must specify an action"
    error_level=1;
fi

if [ $error_level -ne 0 ]; then
    echo "Usage: ${0} 192.168.122.25 24 eth0:1 start|stop"
    exit $error_level
fi

case "${4}" in
    start)
	ip addr show ${3} | grep "${1}/" &> /dev/null
	if [ $? -ne 0 ]; then
	    echo "Adding ${1}/${2} to ${3}"
	    ip addr add ${1}/${2} dev ${3}
	    error_level=$?
	    if [ $error_level -ne 0 ]; then 
		echo "ERROR - Command failed: ip addr add ${1}/${2} dev ${3}"
	    fi
	else 
	    echo "INFO - ${1}/${2} dev ${3} already added" 
	fi
        ;;
    stop)
	ip addr show ${3} | grep "${1}/" &> /dev/null
	if [ $? -eq 0 ]; then
	    echo "Deleting ${1}/${2} to ${3}"
	    ip addr del ${1}/${2} dev ${3}
	    error_level=$?
	    if [ $error_level -ne 0 ]; then 
		echo "ERROR - Command failed: ip addr del ${1}/${2} dev ${3}"
	    fi
	else 
	    echo "INFO - ${1}/${2} dev ${3} already deleted" 
	fi
        ;;
    *)
        echo "ERROR - Action not valid ${4}"
        exit 1
esac

exit $error_level

# End of resource script
