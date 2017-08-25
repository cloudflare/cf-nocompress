local ffi = require('ffi')
local C = ffi.C
local _M = {}

ffi.cdef[[
    void cf_nocompress_set_enabled(void*, unsigned);
]]

function _M.enable(val)
    local r = getfenv(0).__ngx_req
    
    if not r then
        return error("no request found")
    end

    if val == true then
        C.cf_nocompress_set_enabled(r, 1);
    else
        C.cf_nocompress_set_enabled(r, 0);
    end
end

return _M
