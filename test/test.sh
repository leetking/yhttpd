#!/usr/bin/env bash

# test

echo "test"

#yhttpd -r www/ -p 8080 -e www/cgi/ -c 2 -d

./test-set > /dev/null

./test-log

# valgrind --leak-check=yes
