#!/bin/sh 

git commit -m "" ; git push && ssh -x david "(cd RPISensorClient && git pull && cmake . && make )"
