#!/usr/bin/env python3

from flask import Flask
app = Flask(__name__)

prefix='/python'

@app.route(prefix+'/hello-world')
def hello_world():
    return 'Hello World from Python'
@app.route(prefix+'/test-post')
def post():
    return 'Handle deal from Python'

if __name__ == '__main__':
    app.run()
