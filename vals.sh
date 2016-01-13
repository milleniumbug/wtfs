#!/bin/bash
valgrind --vgdb=yes --vgdb-error=0 ./OS_Projekt_WTFS fs -o uid=1000,gid=1000 -d -f asdf
