#!/bin/bash

pid=$(ps -ef | grep test_app4.sh | grep -v grep | awk '{ print $2 }')
if [ ! -z $pid ]; then kill $pid; fi
exit 0
