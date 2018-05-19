#!/usr/bin/env python3

from flask import Flask
from flask import render_template, g, request, session
import sqlite3

cfg = {
    'workspace': '/home/ltk/gits/yhttpd/test/www/fcgi/python_fcgi/',
    'prefix': '/python',
    'db_path': 'users.db',
}

app = Flask(__name__)

PREFIX=cfg['prefix']
DBPATH=cfg['workspace']+cfg['db_path']

def connect_db():
    return sqlite3.connect(DBPATH)

@app.before_request
def before_request():
    g.db = connect_db()

@app.teardown_request
def teardown_request(exception):
    if hasattr(g, 'db'):
        g.db.close()

def is_valid_user(user, pwd):
    ret = False
    if not user or not pwd:
        return False
    cursor = g.db.cursor()
    # enforce (user) as a tuple
    cursor.execute('select * from `users` where `user`=?', (user,))
    rst = cursor.fetchall()
    for i in rst:
        if i[1] == pwd:     # Find
            ret = True
            break

    # Not Find
    cursor.close()
    return ret

def user_have_existed(user):
    ret = False
    if not user:
        return True
    cursor = g.db.cursor()
    cursor.execute('select * from `users` where `user`=?', (user,))
    rst = cursor.fetchall()

    if len(rst) is not 0:
        ret = True

    cursor.close()
    return ret

def add_user(user, pwd):
    if not user or not pwd:
        return False
    # User exist?
    if user_have_existed(user):
        return False
    cursor = g.db.cursor()
    cursor.execute('insert into `users`(`user`,  `password`) values(?,?)',
            (user, pwd))

    cursor.close()
    g.db.commit()
    return True

@app.route(PREFIX+'/')
def index():
    return render_template('index.html')

@app.route(PREFIX+'/login', methods=['POST'])
def login():
    user = request.form.get('name')
    password = request.form.get('password')

    if is_valid_user(user, password):
        return render_template('profile.html', user=user)
    else:
        return render_template('error.html', error="登录失败")

@app.route(PREFIX+'/register', methods=['POST'])
def register():
    user = request.form.get('name')
    password = request.form.get('password')
    if add_user(user, password):
        return render_template('index.html', msg="注册成功")
    else:
        return render_template('error.html', error='注册错误，输入信息错误,或用户存在')

if __name__ == '__main__':
    app.run()
