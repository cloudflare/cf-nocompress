
/*
 * Copyright (C) Yichun Zhang (agentzh)
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include "cf_nocompress_module.h"
#include "cf_nocompress_conf.h"
#include "cf_nocompress_parse.h"
#include "cf_nocompress_flag.h"
#include "cf_nocompress_util.h"
#include "cf_nocompress_out.h"

static const size_t SREGEX_COMPILER_POOL_SIZE = 4096;

static char *cf_nocompress_rule(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static ngx_int_t cf_nocompress_filter_init(ngx_conf_t *cf);

/**
 * Handler to free SRE pools
 */
static void cf_nocompress_cleanup_pool(void *data);

static ngx_command_t cf_nocompress_commands[] = {
    { ngx_string("cf_no_compress"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      cf_nocompress_rule,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    ngx_null_command
};


static ngx_http_module_t  cf_nocompress_module_ctx = {
    NULL,                                  /* preconfiguration */
    cf_nocompress_filter_init,             /* postconfiguration */
    cf_nocompress_create_main_conf,        /* create main configuration */
    NULL,                                  /* init main configuration */
    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */
    cf_nocompress_create_loc_conf,         /* create location configuration */
    cf_nocompress_merge_loc_conf           /* merge location configuration */
};


ngx_module_t cf_nocompress_module = {
    NGX_MODULE_V1,
    &cf_nocompress_module_ctx,             /* module context */
    cf_nocompress_commands,                /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

/**
 * Changes the enabled/disabled state of the module
 * Called from LUA to enable the module
 */
void cf_nocompress_set_enabled(void* r, unsigned enabled) {
    cf_nocompress_loc_conf_t *rlcf;

    dd("change nocompress enabled to %i", enabled);

    rlcf = ngx_http_get_module_loc_conf(
        (ngx_http_request_t*) r,cf_nocompress_module);

    rlcf->enabled = enabled;
}

/**
 * Reset state for the next request
 */
static ngx_int_t
cf_nocompress_header_filter(ngx_http_request_t *r) {

    size_t size;
    ngx_pool_cleanup_t *cln;
    cf_nocompress_ctx_t *ctx;
    cf_nocompress_loc_conf_t *rlcf;

    dd("header filter");

    rlcf = ngx_http_get_module_loc_conf(r, cf_nocompress_module);

    if (rlcf->enabled == 0 || rlcf->regexes.nelts == 0
        || r->headers_out.content_length_n == 0
        || (r->headers_out.content_encoding
            && r->headers_out.content_encoding->value.len)
        || ngx_http_test_content_type(r, &rlcf->types) == NULL)
    {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_pcalloc(r->pool, sizeof(cf_nocompress_ctx_t));

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ctx->last_special = &ctx->special;
    ctx->last_pending = &ctx->pending;
    ctx->last_pending2 = &ctx->pending2;
    ctx->last_captured = &ctx->captured;

    ctx->sub = ngx_pcalloc(r->pool,
        rlcf->regexes.nelts * sizeof(ngx_str_t));
    
    if (ctx->sub == NULL) {
        return NGX_ERROR;
    }

    ctx->ovector = ngx_palloc(r->pool, rlcf->ovecsize);

    if (ctx->ovector == NULL) {
        return NGX_ERROR;
    }

    size = ngx_align(rlcf->regexes.nelts, 8) / 8;
    
    ctx->disabled = ngx_pcalloc(r->pool, size);
    
    if (ctx->disabled == NULL) {
        return NGX_ERROR;
    }

    ctx->vm_pool = sre_create_pool(1024);

    if (ctx->vm_pool == NULL) {
        return NGX_ERROR;
    }

    dd("created vm pool %p", ctx->vm_pool);

    cln = ngx_pool_cleanup_add(r->pool, 0);

    if (cln == NULL) {
        sre_destroy_pool(ctx->vm_pool);
        return NGX_ERROR;
    }

    cln->data = ctx->vm_pool;
    cln->handler = cf_nocompress_cleanup_pool;

    ctx->vm_ctx = sre_vm_pike_create_ctx(ctx->vm_pool,
        rlcf->program,
        ctx->ovector,
        rlcf->ovecsize);
    
    if (ctx->vm_ctx == NULL) {
        return NGX_ERROR;
    }

    ctx->last_out = &ctx->out;

    ngx_http_set_ctx(r, ctx, cf_nocompress_module);

    r->filter_need_in_memory = 1;

    ctx->indices = ngx_array_create(r->pool, 1, sizeof(void*));

    return ngx_http_next_header_filter(r);
}


static void
cf_nocompress_cleanup_pool(void *data) {
    sre_pool_t* pool = data;
    if (pool) {
        dd("destroy sre pool %p", pool);
        sre_destroy_pool(pool);
    }
}

static void
cf_nocompress_update_buf(cf_nocompress_ctx_t* ctx) {
    ctx->pos = ctx->buf->pos;
    ctx->special_buf = ngx_buf_special(ctx->buf);
    ctx->last_buf = (ctx->buf->last_buf || ctx->buf->last_in_chain);
}

static ngx_int_t
cf_nocompress_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {

    ngx_int_t                  rc;
    ngx_buf_t                 *b;
    ngx_chain_t               *cl, *cur = NULL, *rematch = NULL;
    cf_nocompress_ctx_t       *ctx;

    dd("body filter");

    ctx = ngx_http_get_module_ctx(r,cf_nocompress_module);

    /* If there is no context or there is no data to process */
    if (ctx == NULL || (!in && !ctx->buf && !ctx->in && !ctx->busy)) {
        return ngx_http_next_body_filter(r, in);
    }

    if (ctx->vm_done && (!ctx->buf || !ctx->in)) {

        if (cf_nocompress_flush_out(r, ctx) == NGX_ERROR) {
            return NGX_ERROR;
        }

        return ngx_http_next_body_filter(r, in);
    }

    /* Add the incoming chain to the chain ctx->in */
    if (in && ngx_chain_add_copy(r->pool, &ctx->in, in) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log,
        0, "http sub filter \"%V\"", &r->uri);

    /* While there is data in the buffer or data in incoming */
    while (ctx->in || ctx->buf) {

        b = NULL;

        /* If there is no buffer select next input in chain */
        if (ctx->buf == NULL) {
            cur = ctx->in;
            ctx->buf = cur->buf;
            ctx->in = cur->next;

            cf_nocompress_update_buf(ctx);

            dd("=== new incoming buf: size=%d, special=%u, last=%u",
                (int) ngx_buf_size(ctx->buf), ctx->special_buf, ctx->last_buf);
        }

        /* While there is still more data in the buffer
         * or special buffers are still set */
        while (ctx->pos < ctx->buf->last 
            || (ctx->special_buf && ctx->last_buf)) {

            rc = cf_nocompress_parse(r, ctx, rematch);

            ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "replace filter parse: %d, %p-%p", rc,
                 ctx->copy_start, ctx->copy_end);

            if (rc == NGX_ERROR) {
                return rc;
            }

            if (rc == NGX_DECLINED) {

                if (ctx->pending) {
                    *ctx->last_out = ctx->pending;
                    ctx->last_out = ctx->last_pending;

                    ctx->pending = NULL;
                    ctx->last_pending = &ctx->pending;
                }

                if (ctx->special_buf) {
                    ctx->copy_start = NULL;
                    ctx->copy_end = NULL;
                } else {
                    ctx->copy_start = ctx->pos;
                    ctx->copy_end = ctx->buf->last;
                    ctx->pos = ctx->buf->last;
                }

                sre_reset_pool(ctx->vm_pool);
                ctx->vm_done = 1;
            }

            dd("copy_end - copy_start: %d, special: %u",
                (int) (ctx->copy_end - ctx->copy_start), ctx->special_buf);

            if (ctx->copy_start != ctx->copy_end && !ctx->special_buf) {
                dd("copy: %.*s",
                    (int) (ctx->copy_end - ctx->copy_start), ctx->copy_start);

                cl = cf_nocompress_chain(r->pool, &ctx->free);

                if (cl == NULL) {
                    return NGX_ERROR;
                }

                b = cl->buf;

                b->memory = 1;
                b->pos = ctx->copy_start;
                b->last = ctx->copy_end;

                cf_nocompress_out(ctx, cl);
            }

            if (rc == NGX_AGAIN) {
                if (ctx->special_buf && ctx->last_buf) {
                    break;
                }
                continue;
            }

            if (rc == NGX_DECLINED) {
                break;
            }

            /* rc == NGX_OK || rc == NGX_BUSY */

            cl = cf_nocompress_chain(r->pool, &ctx->free);
            
            if (cl == NULL) {
                return NGX_ERROR;
            }

            dd("emit nocompress value: \"%.*s\"",
                (int) (ctx->match_end - ctx->match_start), ctx->match_start);

            cl->buf->memory = 1;
            cl->buf->pos = ctx->match_start;
            cl->buf->last = ctx->match_end;

            cf_nocompress_flag(ctx, cl->buf);
            cf_nocompress_out(ctx, cl);

            if (rc == NGX_BUSY) {
                dd("goto rematch");
                goto rematch;
            }

            if (ctx->special_buf) {
                break;
            }

            continue;
        }

        if ((ctx->buf->flush || ctx->last_buf || ngx_buf_in_memory(ctx->buf))
            && cur) {
            
            if (b == NULL) {
                cl = cf_nocompress_chain(r->pool, &ctx->free);
                
                if (cl == NULL) {
                    return NGX_ERROR;
                }

                b = cl->buf;
                b->sync = 1;

                cf_nocompress_out(ctx, cl);
            }

            dd("setting shadow and last buf: %d", (int) ctx->buf->last_buf);
            b->last_buf = ctx->buf->last_buf;
            b->last_in_chain = ctx->buf->last_in_chain;
            b->flush = ctx->buf->flush;
            b->shadow = ctx->buf;
            b->recycled = ctx->buf->recycled;
        }

        if (ctx->special_buf == 0) {
            ctx->stream_pos += ctx->buf->last - ctx->buf->pos;
        }

        if (rematch) {
            rematch->next = ctx->free;
            ctx->free = rematch;
            rematch = NULL;
        }

rematch:
        /** Rematch Section */
        dd("ctx->rematch: %p", ctx->rematch);

        if (ctx->rematch == NULL) {
            ctx->buf = NULL;
            cur = NULL;
        } else {

            if (cur) {
                ctx->in = cur;
                cur = NULL;
            }

            ctx->buf = ctx->rematch->buf;

            dd("ctx->buf set to rematch buf %p, len=%d, next=%p", ctx->buf,
                (int) ngx_buf_size(ctx->buf), ctx->rematch->next);

            rematch = ctx->rematch;
            ctx->rematch = rematch->next;

            cf_nocompress_update_buf(ctx);
            ctx->stream_pos = ctx->buf->file_pos;
        }
    } /* while */

    return cf_nocompress_flush_out(r, ctx);
}

static char*
cf_nocompress_rule(ngx_conf_t *cf,ngx_command_t *cmd, void *conf) {
    
    dd("cf_nocompress_rule");

    cf_nocompress_loc_conf_t  *rlcf = conf;
    cf_nocompress_main_conf_t *rmcf;
    int                       *flags;
    u_char                   **re;
    ngx_str_t                 *value;
    ngx_pool_cleanup_t        *cln;

    value = cf->args->elts;

    /* Add the regex string to regexes list */
    re = ngx_array_push(&rlcf->regexes);
    
    if (re == NULL) {
        dd(" Error pushing RE");
        return NGX_CONF_ERROR;
    }

    *re = value[1].data;

    /* Flags are always zero in this setup */
    /* TODO: Work out if this is really necessary */
    flags = ngx_array_push(&rlcf->multi_flags);

    if (flags == NULL) {
        return NGX_CONF_ERROR;
    }

    *flags = 0;

    rmcf = ngx_http_conf_get_module_main_conf(cf, cf_nocompress_module);

    if (rmcf->compiler_pool == NULL) {
        
        dd("creating new compiler pool");

        rmcf->compiler_pool = sre_create_pool(SREGEX_COMPILER_POOL_SIZE);
        if (rmcf->compiler_pool == NULL) {
            dd("error creating pool");
            return NGX_CONF_ERROR;
        }

        cln = ngx_pool_cleanup_add(cf->pool, 0);

        if (cln == NULL) {
            dd("error adding pool cleanup handler");
            sre_destroy_pool(rmcf->compiler_pool);
            rmcf->compiler_pool = NULL;
            return NGX_CONF_ERROR;
        }

        cln->data = rmcf->compiler_pool;
        cln->handler = cf_nocompress_cleanup_pool;
    }

    return NGX_CONF_OK;
}

static ngx_int_t
cf_nocompress_filter_init(ngx_conf_t *cf) {

    cf_nocompress_main_conf_t *rmcf;

    dd("init");

    rmcf = ngx_http_conf_get_module_main_conf(cf, cf_nocompress_module);

    /** Store the next filters locally and then install nocompress at top */
    if (rmcf->compiler_pool != NULL) {
        ngx_http_next_header_filter = ngx_http_top_header_filter;
        ngx_http_top_header_filter = cf_nocompress_header_filter;

        ngx_http_next_body_filter = ngx_http_top_body_filter;
        ngx_http_top_body_filter = cf_nocompress_body_filter;
    }

    return NGX_OK;
}