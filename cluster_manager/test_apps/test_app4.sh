#!/bin/bash

cat /dev/null > /tmp/test_app4.out

while :
do
    date >> /tmp/test_app4.out
    sleep 1
done
