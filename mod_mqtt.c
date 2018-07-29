/*
 * mod_mqtt : map http requests to mqtt
 *
 * Klaus Ramstöck
 *
 */

#include <stdio.h>
#include <regex.h>

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


/** Dump a config for debugging
  * \param config - config to dump
  * \return 0
  */
int    DumpCfg(mqtt_config * config) 
{
    DPRINTF ( "-->ctx %s\n", config -> context );
    DPRINTF ( "-->enb %d\n", config -> enabled );
    DPRINTF ( "MQTTServer: %s\n", ( config->mqtt_server ? config->mqtt_server : "(NULL)") );
    DPRINTF ( "MQTTPort: %d\n", config->mqtt_port );
    DPRINTF ( "MQTT PubTopic: %s\n", (config->mqtt_pubtopic ? config->mqtt_pubtopic : "(NULL)") );
    DPRINTF ( "MQTT SubTopic: %s\n", (config->mqtt_subtopic ? config->mqtt_subtopic : "(NULL)") );
    DPRINTF ( "vars: %ld\n",(long int) config ->mqtt_var_table );
    DPRINTF ( "res: %ld\n",(long int) config ->mqtt_var_re_table );
    DPRINTF ( "met: %d\n",(int) config ->methods );
    DPRINTF ( "enc: %d\n",(int) config ->encodings );
return 0;
}

/*
      ==============================================================================
      The hook registration function (also initializes the default config values):
      ==============================================================================
*/

/** register hooks and do init stuff (like setting persistent pool)
  * \param pool - persistent memory pool
  */

void mqtt_register_hooks ( apr_pool_t *pool )
    {
    mqtt_set_pool ( pool );

    DPRINTF ( "--> HOOKS\n" );
    ap_hook_handler ( mqtt_handler, NULL, NULL, APR_HOOK_LAST );
    }

/** create a config object for a directory
  * \param pool - memory pool to use
  * \param context - location for this conf
  * \return config for this dir alone
  */
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
        cfg->mqtt_pubtopic = NULL;
        cfg->mqtt_subtopic = NULL;
        cfg->mqtt_port = -1;
        cfg->mqtt_var_table = NULL;   /* apr_table_copy (pool, const apr_table_t *t); */
        cfg->mqtt_var_re_table = NULL; /* apr_table_copy (apr_pool_t *p, const apr_table_t *t) ; */
        cfg->methods = INVALIDMethod;
        cfg->encodings = INVALIDEncoding;
        }

    DPRINTF ( "<-- dir_conf %s\n", context );
    return cfg;
    }

/** merge base and dir config
  * \param pool - memory pool to use
  * \param BASE
  * \param ADD
  * \return merged config
  */
void *merge_dir_conf ( apr_pool_t *pool, void *BASE, void *ADD )
    {
    mqtt_config *base = ( mqtt_config * ) BASE;                                         /* This is what was set in the parent context */
    mqtt_config *add  = ( mqtt_config * ) ADD;                                          /* This is what is set in the new context */
    mqtt_config *conf = ( mqtt_config * ) create_dir_conf ( pool, add->context );       /* This will be the merged configuration */

    /* Merge configurations */
    DPRINTF ( "--> merge base %s \n", base->context );

    conf->enabled = ( add->enabled < 0 ) ? base->enabled : add->enabled;
    conf->mqtt_port = ( add->mqtt_port < 0 ) ? base->mqtt_port : add->mqtt_port;
    conf->methods = ( add->methods == INVALIDMethod ) ? base->methods : add->methods;
    conf->encodings = ( add->encodings == INVALIDEncoding ) ? base->encodings : add->encodings;

    conf->mqtt_server =  (add->mqtt_server ? add->mqtt_server : base->mqtt_server) ;
   
    conf->mqtt_subtopic =  (add->mqtt_subtopic ? add->mqtt_subtopic : base->mqtt_subtopic) ;
    conf->mqtt_pubtopic =  (add->mqtt_pubtopic ? add->mqtt_pubtopic : base->mqtt_subtopic) ;
   
    if ( add->mqtt_var_table || base->mqtt_var_table )
        conf->mqtt_var_table = apr_table_copy ( pool, (add->mqtt_var_table ? add->mqtt_var_table : base->mqtt_var_table) );

    if ( add->mqtt_var_re_table || base->mqtt_var_re_table )
        conf->mqtt_var_re_table = apr_table_copy ( pool, (add->mqtt_var_re_table ? add->mqtt_var_re_table : base->mqtt_var_re_table) );

    DPRINTF ( "<--merge  \n" );

    return conf;
    }

/*
      ==============================================================================
      Our module handler:
      ==============================================================================
*/

/** handle mqtt requests
  * \param r request to service
  * \return status code
  */
int mqtt_handler ( request_rec *r )
    {

    if ( !r->handler || strcmp ( r->handler, "mqtt-handler" ) )
        return ( DECLINED );

    DPRINTF ( "-->handler\n" );
    mqtt_config *config = ( mqtt_config * )
                          ap_get_module_config ( r->per_dir_config, &mqtt_module );

    DPRINTF ( "-->ctx %s\n", config -> context );

    DumpCfg(config) ;

    keyValuePair *urlData = NULL;
    DPRINTF ( "-->handler2 %s\n", config->context );

    if ( config->encodings != MultiPartEncoding )
        {
        urlData = readUrlArgs ( r );
        }

    keyValuePair *formData = urlData;

    if ( config->encodings != URLEncoding )
        {
        formData = readPost ( r, urlData );
        }

    if ( ! assert_variables(config, formData) )
        {
        return HTTP_BAD_REQUEST;
        }

    const char *pubtopic =  kvSubst ( r->pool, formData, config->mqtt_pubtopic ); 
    const char *subtopic =  kvSubst ( r->pool, formData, config->mqtt_subtopic ); 

        {
        const char * msg = kv2json(r->pool, formData) ;
        int msglen = strlen(msg) ;
        char *response = NULL;
        int responselen ;
        int mqtt_err ;

        struct mosq_config * cfg = NULL ;
        struct mosquitto * mosq = NULL;

	    mqtt_err = mqtt_sub_prepare(r->pool, config->mqtt_server, config->mqtt_port, subtopic, &cfg, &mosq);
	    if ( mqtt_err != MOSQ_ERR_SUCCESS )
		    return HTTP_SERVICE_UNAVAILABLE ;

        mqtt_err = mqtt_pub(r->pool, config->mqtt_server, config->mqtt_port, pubtopic, msg, msglen);
        DPRINTF ( "pub done %d, get resp\n", mqtt_err );
        if (mqtt_err == 0 )
            mqtt_err = mqtt_sub_loop(r->pool, cfg, mosq, &response, &responselen);

        if (response)
            {
            keyValuePair *responseData = json2kv(r->pool, response) ;
            const char * cType = keyValue(responseData, "content-type");
            if ( ! cType )
                return HTTP_INTERNAL_SERVER_ERROR ;
            ap_set_content_type(r, (cType ? cType : "text/html"));
            const char * cData = keyValue(responseData, ".data");
            if ( ! cData )
                return HTTP_INTERNAL_SERVER_ERROR ;
            ap_rwrite(cData, strlen(cData), r);
            }
        else
            {
            DPRINTF ( "No response\n" );
            ap_set_content_type(r, "text/html");
            ap_rprintf(r, "No response, see log\n");
            }
        }
        
    return OK;
    } 

/** assert variables meet constraints configured
 * \param variables in this requet
 * return 1 / OK or 0 / ERROR
 */
int assert_variables(mqtt_config *config, keyValuePair * kvp)
    {
    int i=0 ;
    apr_table_t *vars = config -> mqtt_var_table; 
    apr_table_t *res  = config -> mqtt_var_re_table; 


    DPRINTF ( "-->assert %ld %ld\n", (long int) vars, (long int) res );

    if ( !vars && !res )
        return 1 ; /* Nothing to check */

    DPRINTF ( "-->check keys\n" );

    while(kvp[i].key)
        {
        /* Check key is allowed at all */
        const char *val = apr_table_get(vars, kvp[i].key);
        DPRINTF ( "check %s-> %s : %s\n", kvp[i].key, kvp[i].value, (val ? val : "(NULL)") ) ;
        if ( ! val )
            {
            DPRINTF("Key %s not allowed\n", kvp[i].key );
            return 0; 
            }
        const char *re = apr_table_get(res, kvp[i].key);
        DPRINTF ( "check key value %s against %s\n", kvp[i].value, (re ? re : "(NULL)") ) ;
        if ( ! re )
            {
            DPRINTF("No RE, continue\n" );
            continue ;
            }
        regex_t compiled;
        if(regcomp(&compiled, re, REG_EXTENDED) == 0 )
            {
            int nsub = compiled.re_nsub;
            DPRINTF ( "nsub %d\n", nsub ) ;
            regmatch_t matchptr[nsub+1];
            int err;
            if( (err = regexec (&compiled, kvp[i].value, nsub, matchptr, 0)) )
                {
                char buf[512] ;
                regerror (err, &compiled, buf, sizeof(buf) );
                buf[511] = 0;
                if(err == REG_NOMATCH)
                    {
                    DPRINTF("RE for %s did not match: %s\n", kvp[i].value, buf);
                    return 0;
                    }
                else if(err == REG_ESPACE)
                    {
                    DPRINTF("Ran out of memory.\n");
                    return 0 ;
                    }
                DPRINTF("RE Err: %s\n", buf);
                }
            DPRINTF ( "Matched!\n" ) ;
            regfree(&compiled);
            }
        else
            DPRINTF ( "re %s does not compile\n", re)  ;
        i++;
        }
    DPRINTF ( "-->assert ok\n" );
    return 1 ;
    }
