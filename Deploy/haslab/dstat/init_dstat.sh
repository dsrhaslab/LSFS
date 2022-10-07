#!/bin/bash

mkdir -p /$1

screen -S dstat1 -d -m dstat -tcdmg --noheaders --output /$1/$2