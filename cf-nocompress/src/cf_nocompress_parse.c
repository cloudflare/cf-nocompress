
/*
 * Copyright (C) Yichun Zhang (agentzh)
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include "cf_nocompress_parse.h"
#include "cf_nocompress_util.h"

static inline
void cf_nocompress_parse_flush_pending(cf_nocompress_ctx_t* ctx) {
    if (ctx->pending) {
        *ctx->last_out = ctx->pending;
        ctx->last_out = ctx->last_pending;

        ctx->pending = NULL;
        ctx->last_pending = &ctx->pending;
    }
}

static inline
void cf_nocompress_parse_set_match(cf_nocompress_ctx_t* ctx,
    u_char* from, u_char* to) {
    ctx->match_start = from;
    ctx->match_end = to;
}

static inline
void cf_nocompress_reset_copy(cf_nocompress_ctx_t* ctx) {
    ctx->copy_start = ctx->pos;
    ctx->copy_end = ctx->buf->last;
    ctx->pos = ctx->buf->last;
}

static inline
void cf_nocompress_clear_copy(cf_nocompress_ctx_t* ctx) {
    ctx->copy_start = NULL;
    ctx->copy_end = NULL;
}

static inline 
void cf_nocompress_reset_pending2(cf_nocompress_ctx_t* ctx) {
    ctx->pending2 = NULL;
    ctx->last_pending2 = &ctx->pending2;
}

static inline
void cf_nocompress_free_pending2(cf_nocompress_ctx_t* ctx) {
    *ctx->last_pending2 = ctx->free;
    ctx->free = ctx->pending2;
    cf_nocompress_reset_pending2(ctx);
}

static inline
void cf_nocompress_lastlist_append(ngx_chain_t*** to, ngx_chain_t* item) {
    **to = item;
    *to = &item->next;
}

ngx_int_t cf_nocompress_parse(ngx_http_request_t *r,
    cf_nocompress_ctx_t *ctx, ngx_chain_t *rematch) {

    sre_int_t              ret, from, to, mfrom = -1, mto = -1;
    ngx_int_t              rc;
    ngx_chain_t           *new_rematch = NULL;
    ngx_chain_t           *cl;
    ngx_chain_t          **last_rematch, **last;
    size_t                 len;
    sre_int_t             *pending_matched;

    dd("parse for nocompress");

    if (ctx->vm_done) {
        cf_nocompress_reset_copy(ctx);
        return NGX_AGAIN;
    }

    len = ctx->buf->last - ctx->pos;

    dd("=== process data chunk %p len=%d, pos=%u, special=%u, "
       "last=%u, \"%.*s\"", ctx->buf, (int) (ctx->buf->last - ctx->pos),
       (int) (ctx->pos - ctx->buf->pos + ctx->stream_pos),
       ctx->special_buf, ctx->last_buf,
       (int) (ctx->buf->last - ctx->pos), ctx->pos);

    ret = sre_vm_pike_exec(ctx->vm_ctx, ctx->pos,len,
        ctx->last_buf, &pending_matched);

    dd("vm pike exec: %d", (int) ret);

    if (ret >= 0) {
        ctx->regex_id = ret;
        ctx->total_buffered = 0;

        from = ctx->ovector[0];
        to = ctx->ovector[1];

        dd("pike vm ok: (%d, %d)", (int) from, (int) to);

        cf_nocompress_parse_set_match(ctx,
            ctx->buf->pos + (from - ctx->stream_pos),
            ctx->buf->pos + (to - ctx->stream_pos));

        if (from >= ctx->stream_pos) {

            /* the match is completely on the current buf */
            cf_nocompress_parse_flush_pending(ctx);

            if (ctx->pending2) {
                ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, 
                    "assertion failed: ctx->pending2 is not NULL when "
                    "the match is completely on the current buf");
                return NGX_ERROR;
            }

            ctx->copy_start = ctx->pos;
            ctx->copy_end = ctx->buf->pos + (from - ctx->stream_pos);

            dd("copy len: %d", (int) (ctx->copy_end - ctx->copy_start));

            ctx->pos = ctx->buf->pos + (to - ctx->stream_pos);
            return NGX_OK;
        }

        /* from < ctx->stream_pos */

        if (ctx->pending) {

            if (cf_nocompress_split(r, ctx, &ctx->pending,
                &ctx->last_pending, from, &cl, &last, 0) != NGX_OK) {
                return NGX_ERROR;
            }

            cf_nocompress_parse_flush_pending(ctx);

            if (cl) {
                *last = ctx->free;
                ctx->free = cl;
            }
        }

        if (ctx->pending2) {

            if (cf_nocompress_split(r, ctx, &ctx->pending2,
                &ctx->last_pending2, to, &new_rematch,
                &last_rematch, 1) != NGX_OK) {
                return NGX_ERROR;
            }

            if (ctx->pending2) {
                cf_nocompress_free_pending2(ctx);
            }

            if (new_rematch) {
                if (rematch) {
                    ctx->rematch = rematch;
                }

                /* prepend cl to ctx->rematch */
                *last_rematch = ctx->rematch;
                ctx->rematch = new_rematch;
            }
        }

        cf_nocompress_clear_copy(ctx);

        ctx->pos = ctx->buf->pos + (to - ctx->stream_pos);

        return new_rematch ? NGX_BUSY : NGX_OK;
    }

    switch (ret) {
    case SRE_AGAIN:
        from = ctx->ovector[0];
        to = ctx->ovector[1];

        dd("pike vm again: (%d, %d)", (int) from, (int) to);

        if (from == -1) {
            from = ctx->stream_pos + (ctx->buf->last - ctx->buf->pos);
        }

        if (to == -1) {
            to = ctx->stream_pos + (ctx->buf->last - ctx->buf->pos);
        }

        dd("pike vm again (adjusted): stream pos:%d, (%d, %d)",
           (int) ctx->stream_pos, (int) from, (int) to);

        if (from > to) {
            ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                          "invalid capture range: %i > %i", (ngx_int_t) from,
                          (ngx_int_t) to);
            return NGX_ERROR;
        }

        if (pending_matched) {
            mfrom = pending_matched[0];
            mto = pending_matched[1];

            dd("pending matched: (%ld, %ld)", (long) mfrom, (long) mto);
        }

        if (from == to) {

            if (ctx->pending) {
                ctx->total_buffered = 0;
                cf_nocompress_parse_flush_pending(ctx);
            }

            ctx->copy_start = ctx->pos;
            ctx->copy_end = ctx->buf->pos + (from - ctx->stream_pos);
            ctx->pos = ctx->copy_end;

            return NGX_AGAIN;
        }

        /*
         * append the existing ctx->pending data right before
         * the $& capture to ctx->out.
         */

        if (from >= ctx->stream_pos) {

            /* the match is completely on the current buf */
            ctx->copy_start = ctx->pos;
            ctx->copy_end = ctx->buf->pos + (from - ctx->stream_pos);

            if (ctx->pending) {
                ctx->total_buffered = 0;
                cf_nocompress_parse_flush_pending(ctx);
            }

            if (ctx->pending2) {
                ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                              "assertion failed: ctx->pending2 is not NULL "
                              "when the match is completely on the current "
                              "buf");
                return NGX_ERROR;
            }

            if (pending_matched) {

                if (from < mfrom) {
                    /* create ctx->pending as (from, mfrom) */

                    rc = cf_nocompress_pending(r, ctx, from, mfrom, &cl);

                    if (rc == NGX_ERROR) {
                        return NGX_ERROR;
                    }

                    if (rc == NGX_BUSY) {
                        dd("stop processing because of buffer"
                            " size limit reached");
                        cf_nocompress_reset_copy(ctx);
                        return NGX_AGAIN;
                    }

                    cf_nocompress_lastlist_append(&ctx->last_pending, cl);
                }

                if (mto < to) {
                    /* create ctx->pending2 as (mto, to) */
                    rc = cf_nocompress_pending(r, ctx, mto, to, &cl);

                    if (rc == NGX_ERROR) {
                        return NGX_ERROR;
                    }

                    if (rc == NGX_BUSY) {
                        
                        dd("stop processing because of"
                           "buffer size limit reached");
                        
                        cf_nocompress_reset_copy(ctx);
                        return NGX_AGAIN;
                    }

                    cf_nocompress_lastlist_append(&ctx->last_pending2, cl);
                }

            } else {

                dd("create ctx->pending as (%ld, %ld)",
                    (long) from, (long) to);
                
                rc = cf_nocompress_pending(r, ctx, from, to, &cl);
                
                if (rc == NGX_ERROR) {
                    return NGX_ERROR;
                }

                if (rc == NGX_BUSY) {
                    dd("stop processing because of buffer size limit reached");
                    cf_nocompress_reset_copy(ctx);
                    return NGX_AGAIN;
                }

                cf_nocompress_lastlist_append(&ctx->last_pending, cl);
            }

            ctx->pos = ctx->buf->last;

            return NGX_AGAIN;
        }

        dd("from < ctx->stream_pos");

        if (ctx->pending) {

            /* split ctx->pending into ctx->out and ctx->pending */
            
            if (cf_nocompress_split(r, ctx, &ctx->pending,
                &ctx->last_pending, from, &cl, &last, 1) != NGX_OK) {
                return NGX_ERROR;
            }

            if (ctx->pending) {
                dd("adjust pending: pos=%d, from=%d",
                   (int) ctx->pending->buf->file_pos, (int) from);

                ctx->total_buffered -= (size_t) (from - ctx->pending->buf->file_pos);

                cf_nocompress_parse_flush_pending(ctx);
            }

            if (cl) {
                dd("splitted ctx->pending into ctx->out and ctx->pending: %d",
                    (int) ctx->total_buffered);
                ctx->pending = cl;
                ctx->last_pending = last;
            }

            if (pending_matched && !ctx->pending2 && mto >= ctx->stream_pos) {
                dd("splitting ctx->pending into ctx->pending and ctx->free");

                if (cf_nocompress_split(r, ctx, &ctx->pending,
                    &ctx->last_pending, mfrom, &cl, &last, 0) != NGX_OK) {
                    return NGX_ERROR;
                }

                if (cl) {
                    ctx->total_buffered -= (size_t) (ctx->stream_pos - mfrom);

                    dd("splitted ctx->pending into ctx->pending and ctx->free");
                    *last = ctx->free;
                    ctx->free = cl;
                }
            }
        }

        if (ctx->pending2) {

            if (pending_matched) {
                dd("splitting ctx->pending2 into ctx->free and ctx->pending2");

                if (cf_nocompress_split(r, ctx, &ctx->pending2,
                    &ctx->last_pending2, mto, &cl, &last, 1) != NGX_OK) {
                    return NGX_ERROR;
                }

                if (ctx->pending2) {

                    dd("total buffered reduced by %d (was %d)",
                       (int) (mto - ctx->pending2->buf->file_pos),
                       (int) ctx->total_buffered);

                    ctx->total_buffered -= (size_t)
                        (mto - ctx->pending2->buf->file_pos);

                    cf_nocompress_free_pending2(ctx);
                }

                if (cl) {
                    ctx->pending2 = cl;
                    ctx->last_pending2 = last;
                }
            }

            if (mto < to) {
                dd("new pending data to buffer to ctx->pending2: (%ld, %ld)",
                   (long) mto, (long) to);

                rc = cf_nocompress_pending(r, ctx, mto, to, &cl);

                if (rc == NGX_ERROR) {
                    return NGX_ERROR;
                }

                if (rc == NGX_BUSY) {

                    cf_nocompress_parse_flush_pending(ctx);
                    cf_nocompress_clear_copy(ctx);

                    if (ctx->pending2) {
                        new_rematch = ctx->pending2;
                        last_rematch = ctx->last_pending2;

                        if (rematch) {
                            ctx->rematch = rematch;
                        }

                        /* prepend cl to ctx->rematch */
                        *last_rematch = ctx->rematch;
                        ctx->rematch = new_rematch;

                        cf_nocompress_reset_pending2(ctx);
                    }

                    ctx->pos = ctx->buf->pos + (mto - ctx->stream_pos);
                    return new_rematch ? NGX_BUSY : NGX_OK;
                }

                *ctx->last_pending2 = cl;
                ctx->last_pending2 = &cl->next;
            }

            cf_nocompress_clear_copy(ctx);

            ctx->pos = ctx->buf->last;

            return NGX_AGAIN;
        }

        /* ctx->pending2 == NULL */

        if (pending_matched) {

            if (mto < to) {
                /* new pending data to buffer to ctx->pending2 */
                rc = cf_nocompress_pending(r, ctx, mto, to, &cl);
                if (rc == NGX_ERROR) {
                    return NGX_ERROR;
                }

                if (rc == NGX_BUSY) {
                    cf_nocompress_parse_flush_pending(ctx);
                    cf_nocompress_clear_copy(ctx);
                    ctx->pos = ctx->buf->pos + mto - ctx->stream_pos;
                    return NGX_OK;
                }

                *ctx->last_pending2 = cl;
                ctx->last_pending2 = &cl->next;
            }

            /* otherwise no new data to buffer */

        } else {

            /* new pending data to buffer to ctx->pending */
            rc = cf_nocompress_pending(r, ctx,
                ctx->pos - ctx->buf->pos + ctx->stream_pos, to, &cl);

            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc == NGX_BUSY) {
                cf_nocompress_parse_flush_pending(ctx);
                cf_nocompress_reset_copy(ctx);
                return NGX_AGAIN;
            }

            *ctx->last_pending = cl;
            ctx->last_pending = &cl->next;
        }

        cf_nocompress_clear_copy(ctx);
        ctx->pos = ctx->buf->last;

        return NGX_AGAIN;

    case SRE_DECLINED:
        ctx->total_buffered = 0;
        return NGX_DECLINED;

    default:
        /* SRE_ERROR */
        return NGX_ERROR;
    }

    /* cannot reach here */
}