#include "cf_nocompress_conf.h"

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

extern ngx_module_t  cf_nocompress_module;

void* cf_nocompress_create_main_conf(ngx_conf_t *cf) {
    return ngx_pcalloc(cf->pool, sizeof(cf_nocompress_main_conf_t));
}

void* cf_nocompress_create_loc_conf(ngx_conf_t *cf) {
    cf_nocompress_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(cf_nocompress_loc_conf_t));
    
    if (conf == NULL) {
        return NULL;
    }

    //Enabled by default
    conf->enabled = 1;

    ngx_array_init(&conf->multi_flags, cf->pool, 4, sizeof(int));
    ngx_array_init(&conf->regexes, cf->pool, 4, sizeof(u_char *));

    return conf;
}

char* cf_nocompress_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    u_char         **value;
    sre_int_t        err_offset, err_regex_id;
    ngx_str_t        prefix, suffix;
    sre_pool_t      *ppool; /* parser pool */
    sre_regex_t     *re;
    sre_program_t   *prog;

    cf_nocompress_main_conf_t *rmcf;
    cf_nocompress_loc_conf_t  *prev = parent;
    cf_nocompress_loc_conf_t  *conf = child;

    if (ngx_http_merge_types(cf, &conf->types_keys, &conf->types,
                             &prev->types_keys, &prev->types,
                             ngx_http_html_default_types) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (conf->regexes.nelts > 0 && conf->program == NULL) {

        dd("parsing and compiling %d regexes", (int) conf->regexes.nelts);

        //Sets the default state of the module to a


        ppool = sre_create_pool(1024);

        if (ppool == NULL) {
            return NGX_CONF_ERROR;
        }

        value = conf->regexes.elts;

        /**
         * Compile all parsed regex rules into a single re of alternations
         */ 
        re = sre_regex_parse_multi(ppool, value, conf->regexes.nelts,
                                   &conf->ncaps, conf->multi_flags.elts,
                                   &err_offset, &err_regex_id);

        /**
         * Log any rule failure
         */
        if (re == NULL) {

            if (err_offset >= 0) {
                prefix.data = value[err_regex_id];
                prefix.len = err_offset;

                suffix.data = value[err_regex_id] + err_offset;
                suffix.len = ngx_strlen(value[err_regex_id]) - err_offset;

                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "failed to parse regex at offset %i: "
                                   "syntax error; marked by <-- HERE in "
                                   "\"%V <-- HERE %V\"",
                                   (ngx_int_t) err_offset, &prefix, &suffix);

            } else {

                if (err_regex_id >= 0) {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "failed to parse regex \"%s\"",
                                       value[err_regex_id]);

                } else {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "failed to parse regex \"%s\" "
                                       "and its siblings",
                                       value[0]);
                }
            }

            sre_destroy_pool(ppool);
            return NGX_CONF_ERROR;
        }

        rmcf = ngx_http_conf_get_module_main_conf(cf, cf_nocompress_module);
        prog = sre_regex_compile(rmcf->compiler_pool, re);

        sre_destroy_pool(ppool);

        if (prog == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "failed to compile regex \"%s\" and its siblings", value[0]);
            return NGX_CONF_ERROR;
        }

        conf->program = prog;
        conf->ovecsize = 2 * (conf->ncaps + 1) * sizeof(sre_int_t);
    } else {
        conf->regexes       = prev->regexes;
        conf->multi_flags   = prev->multi_flags;
        conf->program       = prev->program;
        conf->ncaps         = prev->ncaps;
        conf->ovecsize      = prev->ovecsize;
        conf->enabled       = prev->enabled;
    }

    dd("merge Log Finished");

    return NGX_CONF_OK;
}
