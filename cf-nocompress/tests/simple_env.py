#!/usr/bin/env python

import sys
import os

from ngxtest import env

class SimpleEnv(env.NginxEnv):
    _filter_logfile = '/tmp/nginx_filter.log'

    _config = '''
master_process off;
daemon         off;
pid            ./nginx-fl.pid;

error_log /tmp/nginx_error.log {{log_level}};


debug_points stop;

events {
}

http {
    access_log     off;

    lua_package_path    "{{testdir}}/../../lua/?.lua;{{luadir}}/lua-cjson/lua/?.lua";
    lua_package_cpath   "{{luadir}}/lua-cjson/?.so";

    lua_shared_dict metrics 1m;

    server {
        listen       32080 ssl;
        ssl_certificate ../../../t/cert/test.crt;
        ssl_certificate_key ../../../t/cert/test.key;
        server_name  localhost;

        location / {
            default_type 'text/html';
            content_by_lua_block {
                ngx.say('Hello,world! My secret is 12345')
            }
        }

        location /gzip {
            default_type 'text/html';
            cf_no_compress 'A+';
            gzip on;
            content_by_lua_block {
                ngx.say('Hello,world! My secret is 12345')
            }
        }

        location /no_compress_but_not_enabled {
            default_type 'text/html';
            cf_no_compress '[a-zA-Z]+';
            gzip on;
            content_by_lua_block {
                require('cf_nocompress').enable(false);
                ngx.say('AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA');
            }
        }

        location /no_compress_and_is_enabled {
            default_type 'text/html';
            cf_no_compress 'A+';
            gzip on;
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.say('AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA');
            }
        }

        location /deployment_test {
            default_type 'text/html';
            cf_no_compress 'KEY:[0-9]{10}';
            gzip on;
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.say('KEY:121212');
            }
        }

        location /ratio_hit {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'HELLO';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()
                ngx.say("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAHELLOAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")
            }
        }

        location /ratio_hit2 {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'DOGDOGDOG';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()
                ngx.say("HELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDDOGDOGDOGHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLD")
            }
        }

        location /ratio_hit_start {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'DOGDOGDOG';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()
                ngx.say("DOGDOGDOGHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLD")
            }
        }

        location /safe_canary {
            default_type 'text/html';
            gzip on;
            cf_no_compress '[A-Z]+';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAZAMAMAAAAAAAAAAAAAAAAAAAAAN")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:AZAZA")
            }
        }

        location /bad_rule_canary {
            default_type 'text/html';
            gzip on;
            cf_no_compress '[0-9]+';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAZAMAMAAAAAAAAAAAAAAAAAAAAAN")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:AZAZA")
            }
        }

        location /canary {
            default_type 'text/html';
            gzip on;
            cf_no_compress '[A-Z]+';
            content_by_lua_block {
                require('cf_nocompress').enable(false);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAZAMAMAAAAAAAAAAAAAAAAAAAAAN")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:AZAZA")
                ngx.exit(200)
            }
        }

        location /canary2 {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'CrazyCrazyFun:[A-Z]{5}';
            content_by_lua_block {
                require('cf_nocompress').enable(false);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:AZAZA")


                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
            }
        }

        location /safe_canary2 {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'CanaryCrazyFun:[A-Z]{5}';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:AZAZA")


                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
            }
        }

        location /canary2_repeated {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'CanaryCrazyFun:[A-Z]{5}';
            content_by_lua_block {
                require('cf_nocompress').enable(false);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("CanaryCrazyFun:AZAZA")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:AZAZA")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
            }
        }

        location /safe_canary2_repeated {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'CanaryCrazyFun:[A-Z]{5}';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("CanaryCrazyFun:AZAZA")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:AZAZA")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
            }
        }

        location /canary3 {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'CanaryCrazyFun:[A-Z]{5}';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:11222")


                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
            }
        }

        location /safe_canary3 {
            default_type 'text/html';
            gzip on;
            cf_no_compress 'CanaryCrazyFun:[0-9]{5}';
            content_by_lua_block {
                require('cf_nocompress').enable(true);
                ngx.header.content_type = "text/html";
                local args = ngx.req.get_uri_args()

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                
                for key, val in pairs(args) do
                 if type(val) == "table" then
                     ngx.say(key, ":", table.concat(val, ", "))
                 else
                     ngx.say(key, ":", val)
                 end
                end

                ngx.say("CanaryCrazyFun:11222")


                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")

                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
                ngx.say("LOREM IPSUM LOREM LOREM IPSUM LOREM LOREM LOREM LOREM IPSUM LOREM LOREM")
            }
        }
    }
}
'''
    def __init__(self, log_level='warn'):
        # _curdir not populated yet
        curdir = os.path.dirname(sys.modules[self.__module__].__file__)
        pidfile = os.path.abspath(curdir + '/.simple_env.pid')
        # Use this constructor if terminal output is needed
        # super(SimpleEnv, self).__init__(pidfile, log_level=log_level)
        super(SimpleEnv, self).__init__(pidfile, log_level=log_level,
            stdin='/dev/null', stdout='/dev/null', stderr='/dev/null', startup_delay=0.5)
        self.setVar('luadir', os.path.normpath(curdir + '/../../..'))

if __name__ == '__main__':
    SimpleEnv.main()
