#!/usr/bin/env lua

local fastcgi = require("wsapi.fastcgi")

local function hello(wsapi_env)
    local headers = {
        ["Content-type"] = "text/html",
        ["Set-Cookies"] = "Cookie-is-from-fcgi.lua=1",
        ["Server"] = "fcgi.lua",
    }

    local function hello_text()
        local envs = {
            "PATH_INFO",
            "SCRIPT_NAME",
            "SCRIPT_FILENAME",
            "QUERY_STRING",
            "REQUEST_METHOD",
            "CONTENT_TYPE",
            "CONTENT_LENGTH",
            "REQEUST_URI",
            "DOCUMENT_URI",
            "DOCUMENT_ROOT",
            "SERVER_PROTOCOL",
            "REQUEST_SCHEME",
            "GATEWAY_INTERFACE",
            "SERVER_SOFTWARE",
            "REMOTE_ADDR",
            "REMOTE_PORT",
            "SERVER_ADDR",
            "SERVER_PORT",
            "SERVER_NAME",
            "REDIRECT_STATUS",
            "HTTP_COOKIES",
        }
        coroutine.yield("<html><body>")
        coroutine.yield("<p>Hello Wsapi!</p>")
        for _, v in pairs(envs) do
            coroutine.yield(("<p>%s: %s</p>"):format(v, wsapi_env[v]))
        end
        coroutine.yield("</body></html>")
    end

    return 200, headers, coroutine.wrap(hello_text)
end

local function post(wsapi_env)
    local headers = { ["Content-type"] = "text/html" }
    local function res()
    end

    return 200, headers, coroutine.wrap(res)
end

local function run_404(envs)
    local headers = { ["Content-type"] = "text/html" }
    local str = [[
<html>
<center><h1>404 Not Found</h1><center>
<center><p>Server: FastCGI for Lua</p></center>
</html>
]]
    local function text_404()
        coroutine.yield(str)
    end

    return 200, headers, coroutine.wrap(text_404)
end

local function index(envs)
    local headers = { ["Content-type"] = "text/html" }
    local str = [[
    <html>
    <meta http-equiv="refresh" content="0;url=/statics/index.html">
    </html>
    ]]
    local function text_index()
        coroutine.yield(str)
    end

    return 200, headers, coroutine.wrap(text_index)
end

local App = {
    ["/hello"] = hello,
    ["/post"]  = post,
    ["/"] = index,
}

local function run(app)
    fastcgi.run(function (envs)
        local handle = app[envs['SCRIPT_NAME']]
        if not handle then
            handle = run_404
        end
        return handle(envs)
    end)
end

-- GO!
run(App)

