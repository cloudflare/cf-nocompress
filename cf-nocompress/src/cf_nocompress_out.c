#include "cf_nocompress_out.h"

extern ngx_module_t                      cf_nocompress_module;
extern ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

static inline void
cf_nocompress_clear_out(cf_nocompress_ctx_t *ctx) {
    ctx->out = NULL;
    ctx->last_out = &ctx->out;
}

static inline void
cf_nocompress_list_append(ngx_chain_t **to, ngx_chain_t *ref) {
    ngx_chain_t *cl;

    if (*to == NULL) {
        *to = ref;
    } else {
        for (cl = *to; cl->next; cl = cl->next) {}
        cl->next = ref;
    }
}

static inline void
cf_nocompress_clear_busy(ngx_http_request_t *r, cf_nocompress_ctx_t *ctx) {
    ngx_buf_t    *b;
    ngx_chain_t  *cl;

    while (ctx->busy) {

        cl = ctx->busy;
        b = cl->buf;

        if (ngx_buf_size(b) != 0) {
            break;
        }

        if (b->tag != (ngx_buf_tag_t) &cf_nocompress_module) {
            ctx->busy = cl->next;
            ngx_free_chain(r->pool, cl);
        } else {

            if (b->shadow) {
                b->shadow->pos = b->shadow->last;
                b->shadow->file_pos = b->shadow->file_last;
            }

            ctx->busy = cl->next;

            if (ngx_buf_special(b)) {

                /* collect special bufs to ctx->special
                 * as which may still be busy
                 */

                cl->next = NULL;
                *ctx->last_special = cl;
                ctx->last_special = &cl->next;
            } else {
                
                /* add ctx->special to ctx->free because they cannot be busy at
                 * this point */
                *ctx->last_special = ctx->free;
                ctx->free = ctx->special;
                ctx->special = NULL;
                ctx->last_special = &ctx->special;

                /* add the data buf itself to the free buf chain */
                cl->next = ctx->free;
                ctx->free = cl;
            }
        }
    }
}

ngx_int_t cf_nocompress_flush_out(ngx_http_request_t *r,
    cf_nocompress_ctx_t *ctx) {

    ngx_int_t rc;

    /* Short circuit when there is nothing to flush */
    if (ctx->out == NULL && ctx->busy == NULL) {
        return NGX_OK;
    }

    rc = ngx_http_next_body_filter(r, ctx->out);

    /* Add ctx->out to ctx->busy and clear out */
    cf_nocompress_list_append(&ctx->busy, ctx->out);
    cf_nocompress_clear_out(ctx);
    cf_nocompress_clear_busy(r, ctx);

    if (ctx->in || ctx->buf) {
        r->buffered |= NGX_HTTP_SUB_BUFFERED;
    } else {
        r->buffered &= ~NGX_HTTP_SUB_BUFFERED;
    }

    return rc;
}

void cf_nocompress_out(cf_nocompress_ctx_t *ctx, ngx_chain_t *cl) {
    cl->next = NULL;
    *ctx->last_out = cl;
    ctx->last_out = &cl->next;
}