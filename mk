#!/bin/sh 

git commit . -m "update" ; git push && ssh -x david "(cd RPISensorClient && git pull && cmake . && make )"
