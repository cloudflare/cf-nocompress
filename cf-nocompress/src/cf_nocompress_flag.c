#include "cf_nocompress_flag.h"

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

ngx_int_t cf_nocompress_flag(cf_nocompress_ctx_t *ctx,
	ngx_buf_t *buf) {

	void **new_index;

	dd("call into flag");

	new_index = (void**) ngx_array_push(ctx->indices);

	if (new_index == NULL) {
		return NGX_ERROR;
	}

	*new_index = buf;

	dd("flagged buffer");

	return NGX_OK;
}