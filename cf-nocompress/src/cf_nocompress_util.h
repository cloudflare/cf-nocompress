#ifndef _cf_nocompress_UTIL_H_INCLUDED_
#define _cf_nocompress_UTIL_H_INCLUDED_

#include "cf_nocompress_module.h"

/**
 * Grab a new chain
 */
ngx_chain_t * cf_nocompress_chain(ngx_pool_t *p, ngx_chain_t **free);

/**
 * Splits buffers in a chain at a given split index
 * Sets pa and plast_a to the chain before split
 * Sets pb and plast_b to the chain after split
 * If b_sane is set then a new chain is allocated which starts exactly at split
 * If pb and plast_b are NULL then split does not exist in pa
 */
ngx_int_t cf_nocompress_split(ngx_http_request_t *r,
	cf_nocompress_ctx_t *ctx,
	ngx_chain_t **pa,
	ngx_chain_t ***plast_a,
	sre_int_t split,
	ngx_chain_t **pb,
	ngx_chain_t ***plast_b,
	unsigned b_sane);

/**
 * Create a new pending chain (holds pending data until
 * it has failed regex checks) & allocate memory for it
 */
ngx_int_t cf_nocompress_pending(ngx_http_request_t *r,
	cf_nocompress_ctx_t *ctx,
	sre_int_t from,
	sre_int_t to,
	ngx_chain_t **out);

#endif