# Installation instructions

The use of this module requires a small patch to NGINX. Once the patch is applied NGINX can be recompiled with the module using `--add-module=/path/to/cf-nocompress/`.

# Configuration

A regular expression is required to act as an identifier for confidential sections of a webpage. The cf_no_compress directive can be used to specify such regular expressions. Only classical regular expressions can be used, backreferences are unsupported.

# Sample Configuration

Below is a sample NGINX configuration

```
location /my_token {
	gzip on;
	default_type text/html;
	cf_no_compress 'TK[0-9A-F]+';
	content_by_lua '
		local str = require "resty.string"
		ngx.say("...")
	';
}

```