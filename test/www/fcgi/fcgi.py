#!/usr/bin/env python3
#-*- encoding: utf-8 -*-

from flup.server.fcgi import WSGIServer
from python_fcgi.app import app

if __name__ == '__main__':
    #WSGIServer(app, bindAddress='/tmp/xxx.socket').run()
    #WSGIServer(app, bindAddress=('127.0.0.1', 8082)).run()
    WSGIServer(app).run()

