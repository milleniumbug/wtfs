#!/bin/bash
valgrind --vgdb=yes --vgdb-error=0 ./OS_Projekt_WTFS -o uid=1000,gid=1000,allow_other,default_permissions -d -f asdf
