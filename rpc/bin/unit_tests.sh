#!/bin/bash
./unit_test > test_commands; ./bankclient test_commands 
    
for i in `seq 1 10`;
        do
            ./bankclient commands #> /dev/null 2>&1 &
        done

#watch -n 0.5 ./bankclient localhost inquiry 1000

