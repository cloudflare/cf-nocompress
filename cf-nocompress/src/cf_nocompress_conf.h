#ifndef _cf_nocompress_CONF_H_INCLUDED_
#define _cf_nocompress_CONF_H_INCLUDED_
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>
#include <sregex/sregex.h>

typedef struct {
    sre_pool_t              *compiler_pool;
} cf_nocompress_main_conf_t;

typedef struct {
    sre_uint_t                 ncaps;
    size_t                     ovecsize;

    ngx_array_t                regexes;  /* of u_char* */
    ngx_array_t                multi_flags;  /* of int */

    sre_program_t             *program;

    ngx_hash_t                 types;
    ngx_array_t               *types_keys;

    unsigned                   enabled:1;
} cf_nocompress_loc_conf_t;


void* cf_nocompress_create_main_conf(ngx_conf_t *cf);
void* cf_nocompress_create_loc_conf(ngx_conf_t *cf);

/**
 * This will compile all regex rules parsed into a single RE of alternations
 * Ex:
 * nocompress: abc
 * nocompress: d
 * result: (abc)|(d)
 */
char* cf_nocompress_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

#endif