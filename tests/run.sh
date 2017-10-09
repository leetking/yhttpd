#!/usr/bin/env bash

# test

yhttpd -r www/ -p 8080 -e www/cgi/ -c 2 -d

