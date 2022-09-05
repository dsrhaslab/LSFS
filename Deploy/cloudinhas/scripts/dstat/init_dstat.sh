#!/bin/bash

mkdir -p /$1/peer$2

screen -S dstat1 -d -m dstat -tcdmg --noheaders --output /$1/peer$2/$3