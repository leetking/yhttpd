package.path = package.path..";lua_fcgi/?.lua"

local index = require("index")
local api = require("api")

local function status(env)
    local headers = {
        ["Content-type"] = "text/html",
        ["Set-Cookie"] = "from=status",
        ["Server"] = "fcgi.lua",
    }

    local function text()
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
            "HTTP_COOKIE",
        }
        coroutine.yield("<html><body>")
        coroutine.yield("<h1>Meta Variables from <a href='https://github.com/leetking/yhttpd.git'>yhttpd</a></h1>")
        for _, v in pairs(envs) do
            if env[v] then
                coroutine.yield(("<p><b>%s</b>: %s</p>"):format(v, env[v]))
            end
        end
        coroutine.yield("</body></html>")
    end

    return 200, headers, coroutine.wrap(text)
end

return {
    ["/lua/"] = index,
    ["/lua/status"] = status,
    ["/lua/api"]  = api,
}

