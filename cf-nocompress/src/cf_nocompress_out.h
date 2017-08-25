#ifndef _cf_nocompress_OUT_H_INCLUDED_
#define _cf_nocompress_OUT_H_INCLUDED_
#include "cf_nocompress_ctx.h"

/**
 * Add cl to ctx->out
 */
inline void cf_nocompress_out(cf_nocompress_ctx_t *ctx,
	ngx_chain_t *cl);

/**
 * Write ctx->out to the next filter & cleanup
 */
ngx_int_t cf_nocompress_flush_out(ngx_http_request_t *r,
	cf_nocompress_ctx_t *ctx);

#endif