
/*
 * Copyright (C) Yichun Zhang (agentzh)
 */


#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include "cf_nocompress_util.h"

extern ngx_module_t  cf_nocompress_module;

ngx_chain_t* cf_nocompress_chain(ngx_pool_t *p, ngx_chain_t **free) {

    ngx_chain_t *cl = ngx_chain_get_free_buf(p, free);

    if (cl == NULL) {
        return NULL;
    }

    ngx_memzero(cl->buf, sizeof(ngx_buf_t));

    cl->buf->tag = (ngx_buf_tag_t) &cf_nocompress_module;

    return cl;
}

ngx_int_t cf_nocompress_split(ngx_http_request_t *r,
    cf_nocompress_ctx_t *ctx,
    ngx_chain_t **pa,
    ngx_chain_t ***plast_a,
    sre_int_t split,
    ngx_chain_t **pb,
    ngx_chain_t ***plast_b,
    unsigned b_sane) {

    sre_int_t             file_last;
    ngx_chain_t          *cl,
                         *newcl,
                        **ll = pa;

    for (cl = *pa; cl; ll = &cl->next, cl = cl->next) {
        if (cl->buf->file_last > split) {

            /* found an overlap */
            if (cl->buf->file_pos < split) {

                dd("adjust cl buf (b_sane=%d): \"%.*s\"",
                    b_sane, (int) ngx_buf_size(cl->buf), cl->buf->pos);

                file_last = cl->buf->file_last;
                cl->buf->last -= file_last - split;
                cl->buf->file_last = split;

                dd("adjusted cl buf (next=%p): %.*s",
                    cl->next, (int) ngx_buf_size(cl->buf), cl->buf->pos);

                /* build the b chain */
                if (b_sane) {
                    newcl = cf_nocompress_chain(r->pool, &ctx->free);
                    
                    if (newcl == NULL) {
                        return NGX_ERROR;
                    }

                    newcl->buf->memory = 1;
                    newcl->buf->pos = cl->buf->last;
                    newcl->buf->last = cl->buf->last + file_last - split;
                    newcl->buf->file_pos = split;
                    newcl->buf->file_last = file_last;

                    newcl->next = cl->next;

                    *pb = newcl;
                    
                    if (plast_b) {
                        *plast_b = cl->next ? *plast_a : &newcl->next;
                    }

                } else {
                    *pb = cl->next;
                    if (plast_b) {
                        *plast_b = *plast_a;
                    }
                }

                /* truncate the a chain */
                *plast_a = &cl->next;
                cl->next = NULL;

                return NGX_OK;
            }

            /* build the b chain */
            *pb = cl;
            
            if (plast_b) {
                *plast_b = *plast_a;
            }

            /* truncate the a chain */
            *plast_a = ll;
            *ll = NULL;

            return NGX_OK;
        }
    }

    /* missed */
    *pb = NULL;

    if (plast_b) {
        *plast_b = pb;
    }

    return NGX_OK;
}

static inline
ngx_int_t cf_nocompress_pending_buffer(ngx_http_request_t* r,
    cf_nocompress_ctx_t* ctx,
    ngx_buf_t* b,
    size_t len,
    sre_int_t from,
    sre_int_t to) {

    b->start = ngx_palloc(r->pool, len);
    
    if (b->start == NULL) {
        return NGX_ERROR;
    }

    b->temporary = 1;
    b->file_pos = from;
    b->file_last = to;
    b->end = b->start + len;
    b->pos = b->start;
    b->last = ngx_copy(b->pos, ctx->buf->pos + from - ctx->stream_pos, len);

    return NGX_OK;
}

ngx_int_t cf_nocompress_pending(ngx_http_request_t *r,
    cf_nocompress_ctx_t *ctx,
    sre_int_t from,
    sre_int_t to,
    ngx_chain_t **out) {

    size_t       len;
    ngx_chain_t *cl;

    if (from < ctx->stream_pos) {
        from = ctx->stream_pos;
    }

    len = (size_t) (to - from);
    
    if (len == 0) {
        return NGX_ERROR;
    }

    ctx->total_buffered += len;

    /* TODO: If a max size is desired for a buffer it
     * can be checked here against total_buffered*/

    /* Create a new chain for the pending buffer */
    cl = cf_nocompress_chain(r->pool, &ctx->free);
    
    if (cl == NULL) {
        return NGX_ERROR;
    }

    ngx_int_t res = cf_nocompress_pending_buffer(r,
        ctx,
        cl->buf,
        len, from, to);

    if (res != NGX_OK) {
        return res;
    }

    dd("buffered pending data: stream_pos=%ld (%ld, %ld): %.*s",
        (long) ctx->stream_pos,
        (long) from, (long) to,
        (int) len,
        ctx->buf->pos + from - ctx->stream_pos);
    
    *out = cl;
    return NGX_OK;
}