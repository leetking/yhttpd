#!/usr/bin/env lua

local fastcgi = require("wsapi.fastcgi")
local app = require("lua_fcgi.app")

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

    return 404, headers, coroutine.wrap(text_404)
end


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
run(app)

