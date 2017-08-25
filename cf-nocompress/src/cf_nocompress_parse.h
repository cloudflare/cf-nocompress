#ifndef _cf_nocompress_PARSE_H_INCLUDED_
#define _cf_nocompress_PARSE_H_INCLUDED_
#include "cf_nocompress_ctx.h"

ngx_int_t cf_nocompress_parse(ngx_http_request_t *r,
    cf_nocompress_ctx_t *ctx, ngx_chain_t *rematch);

#endif