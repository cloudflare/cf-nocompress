#ifndef _cf_nocompress_CTX_H_INCLUDED_
#define _cf_nocompress_CTX_H_INCLUDED_

#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>
#include <sregex/sregex.h>

typedef struct {

    sre_int_t                  regex_id;
    sre_int_t                  stream_pos;
    sre_int_t                 *ovector;
    sre_pool_t                *vm_pool;
    sre_vm_pike_ctx_t         *vm_ctx;

    /* Pending data before the match */
    ngx_chain_t               *pending;
    ngx_chain_t              **last_pending;

    /* Pending data after the match */
    ngx_chain_t               *pending2;
    ngx_chain_t              **last_pending2;

    ngx_buf_t                 *buf;

    ngx_str_t                 *sub;

    u_char                    *pos;

    u_char                    *copy_start;
    u_char                    *copy_end;
    u_char                    *match_start;
    u_char                    *match_end;

    ngx_chain_t               *in;
    ngx_chain_t               *out;
    ngx_chain_t              **last_out;
    ngx_chain_t               *busy;
    ngx_chain_t               *free;
    ngx_chain_t               *special;
    ngx_chain_t              **last_special;
    ngx_chain_t               *rematch;
    ngx_chain_t               *captured;
    ngx_chain_t              **last_captured;
    uint8_t                   *disabled;
    sre_uint_t                 disabled_count;

    size_t                     total_buffered;

    unsigned                   vm_done:1;
    unsigned                   special_buf:1;
    unsigned                   last_buf:1;

    ngx_array_t                *indices;
} cf_nocompress_ctx_t;

#endif