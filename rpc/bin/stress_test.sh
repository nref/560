#!/bin/bash

./unit_test > utests;
for i in `seq 1 100`;
        do
            ./bankclient utests > /dev/null 2>&1 &
        done

watch -n 0.5 cat accounts

