#!/usr/bin/bash

CURRENT_PATH=`pwd`

FCGI_PATH=$CURRENT_PATH/fcgi

#LISTENING=/tmp/fcgi.lua.socket
LISTENING=127.0.0.1:8081

LUA_FCGI=fcgi.lua

#spawn-fcgi -d $FCGI_PATH -s $LISTENING -- $LUA_FCGI
spawn-fcgi -d $FCGI_PATH -a 127.0.0.1 -p 8081 -- $LUA_FCGI
