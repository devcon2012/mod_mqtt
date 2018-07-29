
/*
 * mod_mqtt : map http requests to mqtt
 *
 * Klaus Ramstöck
 *
 */

#ifndef _MOD_MQTT_H
#define _MOD_MQTT_H

#include "apr.h"
#include "apr_tables.h"
#include "ap_config.h"
#include "ap_provider.h"
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"
#include "keyValuePair.h"

/*
  ==============================================================================
  Our configuration prototype and declaration:
  ==============================================================================
*/

typedef enum _Methods
{
    ALLMethods = 0,
    GETMethod = 1,
    POSTMethod = 2,
    INVALIDMethod = 128
} Methods;

typedef enum _Encodings
{
    ALLEncodings = 0,
    URLEncoding = 1,
    MultiPartEncoding = 2,
    INVALIDEncoding = 128
} Encodings;

/* Allow max 20 vars in MQTTVariables */
#define MQTT_MAX_VARS 20

typedef struct
{
    char context[256];
    int enabled;                        /* Enable or disable our module */
    const char *mqtt_server;            /* MQTT Server spec */
    int mqtt_port;                      /* MQTT Server port */
    const char * mqtt_pubtopic;         /* MQTT Server publish topic for query */
    const char * mqtt_subtopic;         /* MQTT Server subscribe topic for answer */
    apr_table_t * mqtt_var_table;       /* MQTT variables send , eg "MQTTVariables   Id Name Action" */
    apr_table_t * mqtt_var_re_table;    /* MQTT variables check regexpressions, 'MQTTCheckVariable Action ^submit|receive$' */
    Methods methods;                    /* Methods serviced (GET POST ALL) eg MQTTMethods ALL */
    Encodings encodings;                /* Allowed enctypes for post: application/x-www-form-urlencoded, multipart/form-data, ALL */
} mqtt_config;

/* Handler for the "MQTTEnabled" directive */
const char *mqtt_set_enabled(cmd_parms *cmd, void *cfg, const char *arg);

/* Handler for the "MQTTServer" directive */
const char *mqtt_set_server(cmd_parms *cmd, void *cfg, const char *arg);

/* Handler for the "MQTTPubTopic" directive */
const char *mqtt_set_pubtopic(cmd_parms *cmd, void *cfg, const char *arg);

/* Handler for the "MQTTSubTopic" directive */
const char *mqtt_set_subtopic(cmd_parms *cmd, void *cfg, const char *arg);

/* Handler for the "MQTTPort" directive */
const char *mqtt_set_port(cmd_parms *cmd, void *cfg, const char *arg);

/* Handler for the "MQTTVariables" directive */
const char *mqtt_set_variables(cmd_parms *cmd, void *cfg, const char *arg);

/* Handler for the "MQTTCheckVariable" directive */
const char *mqtt_set_variable_checks(cmd_parms *cmd, void *cfg, const char *arg1, const char *arg2);

/* Handler for the "MQTTMethods" directive */
const char *mqtt_set_methods(cmd_parms *cmd, void *cfg, const char *arg);

/* Handler for the "MQTTEnctype" directive */
const char *mqtt_set_encodings(cmd_parms *cmd, void *cfg, const char *arg);

/* acessors for variaous global objects */
apr_pool_t *mqtt_get_pool();
apr_pool_t *mqtt_set_pool(apr_pool_t *p);

int mqtt_handler(request_rec *r);
void mqtt_register_hooks(apr_pool_t *pool);
void *create_dir_conf(apr_pool_t *pool, char *context);
void *merge_dir_conf(apr_pool_t *pool, void *BASE, void *ADD);

/* */
int assert_variables(mqtt_config *config, keyValuePair * formdata);

/*
        ==============================================================================
        The directive structure for our name tag:
        ==============================================================================
*/

extern const command_rec mqtt_directives[];

#endif