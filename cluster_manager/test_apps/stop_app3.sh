#!/bin/bash

pid=$(ps -ef | grep test_app3.sh | grep -v grep | awk '{ print $2 }')
if [ ! -z $pid ]; then kill $pid; fi
exit 0
