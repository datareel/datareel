#!/bin/bash

cat /dev/null > /tmp/test_app1.out

while :
do
    date >> /tmp/test_app1.out
    sleep 1
done
