/*
 * mod_mqtt : map http requests to mqtt
 *
 * Klaus Ramstöck
 *
 */

#include <stdio.h>

#include "mod_mqtt.h"
#include "mqtt_common.h"

/*
      ==============================================================================
      Our module name tag:
      ==============================================================================
*/

module AP_MODULE_DECLARE_DATA mqtt_module =
    {
    STANDARD20_MODULE_STUFF,
    create_dir_conf,    /* Per-directory configuration handler */
    merge_dir_conf,     /* Merge handler for per-directory configurations */
    NULL,               /* Per-server configuration handler */
    NULL,               /* Merge handler for per-server configurations */
    mqtt_directives,    /* Any directives we may have for httpd */
    mqtt_register_hooks /* Our hook registering function */
    };

/*
      ==============================================================================
      The hook registration function (also initializes the default config values):
      ==============================================================================
*/

void mqtt_register_hooks ( apr_pool_t *pool )
    {
    mqtt_set_pool ( pool );

    DPRINTF ( "--> HOOKS\n" );
    ap_hook_handler ( mqtt_handler, NULL, NULL, APR_HOOK_LAST );
    }

void *create_dir_conf ( apr_pool_t *pool, char *context )
    {
    context = context ? context : "(undefined context)";

    DPRINTF ( "--> dir_conf %s\n", context );

    mqtt_config *cfg = apr_pcalloc ( pool, sizeof ( mqtt_config ) );

    if ( cfg )
        {
        /* Set some default values */
        strcpy ( cfg->context, context );
        cfg->enabled = -1;
        cfg->mqtt_server = NULL;
        cfg->mqtt_uri = NULL;
        cfg->mqtt_port = -1;
        cfg->mqtt_var_array = NULL;   /* apr_table_copy (pool, const apr_table_t *t); */
        cfg->mqtt_var_re_hash = NULL; /* apr_table_copy (apr_pool_t *p, const apr_table_t *t) ; */
        cfg->methods = INVALIDMethod;
        cfg->encodings = INVALIDEncoding;
        }

    DPRINTF ( "<-- dir_conf %s\n", context );
    return cfg;
    }

void *merge_dir_conf ( apr_pool_t *pool, void *BASE, void *ADD )
    {
    mqtt_config *base = ( mqtt_config * ) BASE;                                       /* This is what was set in the parent context */
    mqtt_config *add = ( mqtt_config * ) ADD;                                         /* This is what is set in the new context */
    mqtt_config *conf = ( mqtt_config * ) create_dir_conf ( pool, "Merged configuration" ); /* This will be the merged configuration */

    /* Merge configurations */
    DPRINTF ( "--> merge base %s \n", base->context );

    conf->enabled = ( add->enabled < 0 ) ? base->enabled : add->enabled;
    conf->mqtt_port = ( add->mqtt_port < 0 ) ? base->mqtt_port : add->mqtt_port;
    conf->methods = ( add->methods == INVALIDMethod ) ? base->methods : add->methods;
    conf->encodings = ( add->encodings == INVALIDEncoding ) ? base->encodings : add->encodings;

    if ( add->mqtt_server || base->mqtt_server )
        conf->mqtt_server = apr_pstrdup ( pool, add->mqtt_server ? add->mqtt_server : base->mqtt_server );

    if ( add->mqtt_uri || base->mqtt_uri )
        conf->mqtt_uri = apr_pstrdup ( pool, add->mqtt_uri ? add->mqtt_uri : base->mqtt_uri );

    if ( add->mqtt_var_array || base->mqtt_var_array )
        conf->mqtt_var_array = apr_table_copy ( pool, add->mqtt_var_array ? add->mqtt_var_array : base->mqtt_var_array );

    if ( add->mqtt_var_re_hash || base->mqtt_var_re_hash )
        conf->mqtt_var_re_hash = apr_table_copy ( pool, add->mqtt_var_re_hash ? add->mqtt_var_re_hash : base->mqtt_var_re_hash );

    DPRINTF ( "<--merge  \n" );

    return add;
    }

/*
      ==============================================================================
      Our module handler:
      ==============================================================================
*/

int mqtt_handler ( request_rec *r )
    {
    if ( !r->handler || strcmp ( r->handler, "mqtt-handler" ) )
        return ( DECLINED );

    DPRINTF ( "-->handler\n" );
    mqtt_config *config = ( mqtt_config * )
                          ap_get_module_config ( r->per_dir_config, &mqtt_module );

    ap_set_content_type ( r, "text/plain" );
    ap_rprintf ( r, "MQTTEnabled: %u\n", config->enabled );
    ap_rprintf ( r, "MQTTServer: %s\n", config->mqtt_server );
    ap_rprintf ( r, "MQTTPort: %d\n", config->mqtt_port );
    ap_rprintf ( r, "MQTTURI: %s\n", config->mqtt_uri );
    ap_rprintf ( r, "URI: %s\n", r->uri );
    ap_rprintf ( r, "ARGS: %s\n", ( r->args ? r->args : "(NULL)" ) );

    keyValuePair *urlData = NULL;
    DPRINTF ( "-->handler2 %s\n", config->context );

    if ( config->encodings != MultiPartEncoding )
        {
        urlData = readUrlArgs ( r );

        if ( urlData )
            {
            int i;

            for ( i = 0; &urlData[i]; i++ )
                {
                if ( urlData[i].key && urlData[i].value )
                    {
                    ap_rprintf ( r, "%s = %s\n", urlData[i].key, urlData[i].value );
                    }
                else if ( urlData[i].key )
                    {
                    ap_rprintf ( r, "%s\n", urlData[i].key );
                    }
                else if ( urlData[i].value )
                    {
                    ap_rprintf ( r, "= %s\n", urlData[i].value );
                    }
                else
                    {
                    break;
                    }
                }
            }
        }

    keyValuePair *formData = urlData;

    if ( config->encodings != URLEncoding )
        {
        formData = readPost ( r, urlData );

        if ( formData )
            {
            int i;

            for ( i = 0; &formData[i]; i++ )
                {
                if ( formData[i].key && formData[i].value )
                    {
                    ap_rprintf ( r, "%s = %s\n", formData[i].key, formData[i].value );
                    }
                else if ( formData[i].key )
                    {
                    ap_rprintf ( r, "%s\n", formData[i].key );
                    }
                else if ( formData[i].value )
                    {
                    ap_rprintf ( r, "= %s\n", formData[i].value );
                    }
                else
                    {
                    break;
                    }
                }
            }
        }
        if ( ! assert_variables(config, formData) )
            {
            return HTTP_BAD_REQUEST;
            }

    const char *action = keyValue ( urlData, "action" );
    DPRINTF ( "-->handler3 %s\n", ( action ? action : "(NULL)" ) );

    const char *topic = keySubst ( urlData, r->pool, "action", config->mqtt_uri );
    ap_rprintf ( r, "uri = %s\n", topic );

    return OK;
    }

/** assert variables meet constraints configured
 * 
 * 
 * 
 */
int assert_variables(mqtt_config *config, keyValuePair * kvp)
    {
    int i=0 ;
    apr_table_t *mqtt_var_array;  /* MQTT variables send , eg "MQTTVariables   Id Name Action" */
    apr_hash_t *mqtt_var_re_hash; /* MQTT variables check regexpressions, 'MQTTCheckVariable Action ^submit|receive$' */
   
    while(kvp[i].key)
        {
        /* Check key is allowed at all */
        i++;
        }
    return 1 ;
    }
