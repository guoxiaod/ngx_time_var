#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    unsigned       begin:5;
    unsigned       len:3;
} ngx_http_time_item_t;

typedef struct {
    ngx_int_t  hour_period;
} ngx_http_time_var_conf_t;

static ngx_int_t ngx_http_time_string_variable(
    ngx_http_request_t *r,
    ngx_http_variable_value_t *v,
    uintptr_t data);
static ngx_int_t ngx_http_time_item_variable(
    ngx_http_request_t *r,
    ngx_http_variable_value_t *v,
    uintptr_t data);
static ngx_int_t ngx_http_time_tomsec_variable(
    ngx_http_request_t *r,
    ngx_http_variable_value_t *v,
    uintptr_t data);
static ngx_int_t ngx_http_time_tosec_variable(
    ngx_http_request_t *r,
    ngx_http_variable_value_t *v,
    uintptr_t data);

static ngx_int_t ngx_http_time_to_hour_period_variable(
    ngx_http_request_t *r,
    ngx_http_variable_value_t *v,
    uintptr_t data);

static void *ngx_http_time_var_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_time_var_merge_loc_conf(ngx_conf_t *cf, void *parent,
        void *child);


static ngx_int_t ngx_http_time_add_vars(ngx_conf_t *cf);

static ngx_command_t ngx_http_time_var_commands[] = { 
    { ngx_string("time_var_hour_period"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_time_var_conf_t, hour_period),
        NULL},

    ngx_null_command
};

static ngx_http_module_t  ngx_http_time_var_module_ctx = {
    ngx_http_time_add_vars,        /* preconfiguration */
    NULL,                          /* postconfiguration */
    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */
    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */
    ngx_http_time_var_create_loc_conf,/* create location configuration */
    ngx_http_time_var_merge_loc_conf/* merge location configuration */
};

ngx_module_t  ngx_http_time_var_module = {
    NGX_MODULE_V1,
    &ngx_http_time_var_module_ctx,
    ngx_http_time_var_commands,
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

// acquire item value by using ngx_cached_err_log_time
static ngx_http_time_item_t ngx_http_time_item_info[] = {
    {0,4}, {5,2}, {8,2}, {11,2}, {14,2}, {17,2},
};

static ngx_http_variable_t  ngx_http_time_vars[] = {

    //format: 1970/09/28 12:00:00
    { ngx_string("tm_err_log_time"), NULL,
      ngx_http_time_string_variable,
      (uintptr_t)&ngx_cached_err_log_time,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    //format: Mon, 28 Sep 1970 06:00:00 GMT
    { ngx_string("tm_http_time"), NULL,
      ngx_http_time_string_variable,
      (uintptr_t)&ngx_cached_http_time,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    //format: 28/Sep/1970:12:00:00 +0600
    { ngx_string("tm_http_log_time"), NULL,
      ngx_http_time_string_variable,
      (uintptr_t)&ngx_cached_http_log_time,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    //format: 1970-09-28T12:00:00+06:00
    { ngx_string("tm_http_log_iso8601"), NULL,
      ngx_http_time_string_variable,
      (uintptr_t)&ngx_cached_http_log_iso8601,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_year"), NULL,
      ngx_http_time_item_variable,
      (uintptr_t)&ngx_http_time_item_info[0],
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_month"), NULL,
      ngx_http_time_item_variable,
      (uintptr_t)&ngx_http_time_item_info[1],
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_day"), NULL,
      ngx_http_time_item_variable,
      (uintptr_t)&ngx_http_time_item_info[2],
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_hour"), NULL,
      ngx_http_time_item_variable,
      (uintptr_t)&ngx_http_time_item_info[3],
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_minute"), NULL,
      ngx_http_time_item_variable,
      (uintptr_t)&ngx_http_time_item_info[4],
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_second"), NULL,
      ngx_http_time_item_variable,
      (uintptr_t)&ngx_http_time_item_info[5],
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_tosec"), NULL,
      ngx_http_time_tosec_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_tomsec"), NULL,
      ngx_http_time_tomsec_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tm_hour_period"), NULL,
      ngx_http_time_to_hour_period_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};

static ngx_int_t
ngx_http_time_add_vars(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;
    for (v = ngx_http_time_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_time_string_variable(ngx_http_request_t *r,
                              ngx_http_variable_value_t *v,
                              uintptr_t data)
{
    ngx_str_t *str = (ngx_str_t *)data;
    v->len = str->len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = str->data;
    return NGX_OK;
}

static ngx_int_t
ngx_http_time_item_variable(ngx_http_request_t *r,
                            ngx_http_variable_value_t *v,
                            uintptr_t data)
{
    ngx_http_time_item_t *info = (ngx_http_time_item_t *)data;
    v->len = info->len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ngx_cached_err_log_time.data + info->begin;
    return NGX_OK;
}

static ngx_int_t
ngx_http_time_tomsec_variable(ngx_http_request_t *r,
                              ngx_http_variable_value_t *v,
                              uintptr_t data)
{
    ngx_time_t    *tp;
    u_char        *p;

    tp = ngx_timeofday();

    p = ngx_pnalloc(r->pool, NGX_TIME_T_LEN + 4);
    if (p == NULL) {
        return NGX_ERROR;
    }
    v->len = ngx_sprintf(p, "%T.%03M", tp->sec, tp->msec) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;
    return NGX_OK;
}

static ngx_int_t
ngx_http_time_tosec_variable(ngx_http_request_t *r,
                              ngx_http_variable_value_t *v,
                              uintptr_t data)
{
    ngx_time_t    *tp;
    u_char        *p;

    tp = ngx_timeofday();

    p = ngx_pnalloc(r->pool, NGX_TIME_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }
    v->len = ngx_sprintf(p, "%T", tp->sec) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;
    return NGX_OK;
}

static ngx_int_t ngx_http_time_to_hour_period_variable(
    ngx_http_request_t *r,
    ngx_http_variable_value_t *v,
    uintptr_t data) 
{
    ngx_int_t     hour, period;
    ngx_time_t    *tp;
    u_char        *p;

    ngx_http_time_var_conf_t * conf;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_time_var_module);
    period = (conf->hour_period == 2 || conf->hour_period == 3 || conf->hour_period == 4
        || conf->hour_period == 6 || conf->hour_period == 8 || conf->hour_period == 12) ? conf->hour_period : 1;

    tp = ngx_timeofday();

    p = ngx_pnalloc(r->pool, 3);
    if (p == NULL) {
        return NGX_ERROR;
    }

    hour = ((int)(tp->sec / 3600) + 8) % 24;
    hour = hour - (hour % period);

    v->len = ngx_sprintf(p, "%02d", hour) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;
    return NGX_OK;
}

static void *ngx_http_time_var_create_loc_conf(ngx_conf_t *cf) {
    ngx_http_time_var_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_time_var_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->hour_period = NGX_CONF_UNSET_UINT;
    return conf;
}
static char *ngx_http_time_var_merge_loc_conf(ngx_conf_t *cf, void *parent,
        void *child) {

    ngx_http_time_var_conf_t *prev = parent;
    ngx_http_time_var_conf_t *conf = child;

    ngx_conf_merge_value(conf->hour_period, prev->hour_period, 1); 

    return NGX_CONF_OK;
}
