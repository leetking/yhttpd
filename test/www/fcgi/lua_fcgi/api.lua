local function post(env)
    local headers = { ["Content-type"] = "text/html" }
    local function res()
        local input = env.input:read()
        local age
        local args = {}
        for k, v in input:gmatch("([^=&]+)=([^&]+)") do
            args[k] = v
        end
        local cookie = env["HTTP_COOKIE"]

        local template = [[
        <html>
        <title>Your Porfile</title>
        <body>
            <p>Name: %s</p>
            <p>Age: %s</p>
            <p>cookie: %s</p>
            <a href='/lua/'>back</a>
        </body>
        </html>
        ]]

        if not args['name'] then
            args['name'] = "You MUST type your name"
        end

        if not args['age'] then
            args['age'] = "You MUST type your age"
        else
            age = tonumber(args['age'])
            if age < 0 or age > 200 then
                args['age'] = "You aren't human!"
            end
        end
        coroutine.yield(template:format(args['name'], args['age'], cookie))
    end

    return 200, headers, coroutine.wrap(res)
end

local function get(env)
    local headers = {
        ["Content-Type"] = "text/html",
    }
    
    local function res()
        local query = env['QUERY_STRING']
        coroutine.yield("<center><p>from fcgi.lua get method</p></center>")
        coroutine.yield("Your query string is "..query)
    end

    return 200, headers, coroutine.wrap(res)
end

local function unsupport_method(env)
    local function res()
        coroutine.yield([[
        <html><title>501 Unimplement Method</title>
        <body>
            <h1>Unimplement Method</h1>
            <p>
            api version: v0.0.1
            </p>
        </body>
        </html>
        ]])
    end
    return 501, {["Content-Type"] = "text/html"}, coroutine.wrap(res)
end

local function api(env)
    if "GET" == env["REQUEST_METHOD"] then
        return get(env)
    elseif "POST" == env["REQUEST_METHOD"] then
        return post(env)
    else
        return unsupport_method(env)
    end
end

return setmetatable({}, {
    __call = function(this, args)
        return api(args)
    end,
})

