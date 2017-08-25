#ifndef _cf_nocompress_flag_H_INCLUDED_
#define _cf_nocompress_flag_H_INCLUDED_
#include "cf_nocompress_module.h"

/**
 * Flagging a buffer marks it to not be compressed
 */
ngx_int_t cf_nocompress_flag(cf_nocompress_ctx_t *ctx,
	ngx_buf_t *buf);

#endif