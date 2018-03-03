#!/bin/sh 

build_host=orangepi


git commit . -m "update" ; git push && ssh -x $buildhost "(cd RPISensorClient && git pull && cmake . && make )"
