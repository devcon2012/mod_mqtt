
/*
 * keyValuePair : hash operations + similar stuff
 *
 * Klaus Ramstöck
 *
 */

#ifndef _KEYPAIR_H
#define _KEYPAIR_H

#include "mqtt_common.h"
#include "http_request.h"

typedef struct
{
    const char *key;
    const char *value;
} keyValuePair;

keyValuePair *readUrlArgs(request_rec *r);
keyValuePair *readPost(request_rec *r, keyValuePair *other);
const char *keyValue(keyValuePair *kvp, const char *key);
const char *keySubst(apr_pool_t *p, keyValuePair *kvp, const char *key, const char *tgt);
const char * kvSubst (apr_pool_t *p, keyValuePair *kvp, const char *tgt );
char * xstrdup(apr_pool_t *p, const char *src);
const char * kv2json(apr_pool_t *p, keyValuePair *vars);
keyValuePair * json2kv(apr_pool_t *p, const char *json);

#endif
