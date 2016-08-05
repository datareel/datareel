#!/bin/bash

cat /dev/null > /tmp/test_app2.out

while :
do
    date >> /tmp/test_app2.out
    sleep 1
done
