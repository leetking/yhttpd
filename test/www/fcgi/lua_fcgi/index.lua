local body = [[
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>FastCGI.lua</title>
  <style>
    .center {
      margin: auto;
    }
    .main {
      width: 80%;
    }
    .align {
      display: inline-block;
      width: 5rem;
    }
    .word {
    }
  </style>
</head>
<body>
  <div class='center main'>
  <h1>Dynamic web site powered by fcgi.lua</h1>
  <h2>I. Post Data to server</h2>
  <form action="/lua/api" method="post">
    <span class='align word'>Name:</span><input name="name" type="text" /><br />
    <span class='align word'>Age: </span><input name="age" type="text" /><br />
    <input type="submit" value="submit" />
  </form>

  <h2>II. Some links</h2>
  <ol>
    <li><a href='/lua/'>index</a> This page</li>
    <li><a href='/lua/status'>status</a> List all Meta Variables from server yhttpd</li>
    <li><a href='/'>go back</a></li>
  </ol>
</body>
</html>
]]
local function index(env)
    local headers = {
        ["Content-Type"] = "text/html",
        ["Set-Cookie"] = "from=fcgi.lua-index";
    }
    return 200, headers, coroutine.wrap(function() coroutine.yield(body) end)
end

return setmetatable({}, {
    __call = function(this, env)
        return index(env)
    end,
})
