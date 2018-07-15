/*
 * mod_mqtt : map http requests to mqtt
 *
 * Klaus Ramstöck
 *
 */

#include <stdio.h>

#include "mod_mqtt.h"
#include "mqtt_common.h"
#include "keyValuePair.h"

/*
    ==============================================================================
    Our mod_mqtt directives
    ==============================================================================
*/

/** Apache configuration directives
 */
const command_rec mqtt_directives[] =
    {
    AP_INIT_TAKE1("MQTTPort", mqtt_set_port, NULL, OR_ALL,
                  "MQTT Server port"),
    AP_INIT_TAKE1("MQTTURI", mqtt_set_uri, NULL, OR_ALL,
                  "MQTT Server URI"),
    AP_INIT_TAKE1("MQTTServer", mqtt_set_server, NULL, OR_ALL,
                  "MQTT Server"),
    AP_INIT_TAKE1("MQTTEnabled", mqtt_set_enabled, NULL, OR_ALL,
                  "Enable MQTT Bridge"),
    AP_INIT_ITERATE("MQTTVariables", mqtt_set_variables, NULL, OR_ALL,
                    "Allowed variables (use - for all)"),
    AP_INIT_TAKE2("MQTTCheckVariable", mqtt_set_variable_checks, NULL, OR_ALL,
                  "Variable check REs"),
    AP_INIT_TAKE1("MQTTMethods", mqtt_set_methods, NULL, OR_ALL,
                  "Set form methods considered: GET POST ALL"),
    AP_INIT_TAKE1("MQTTEnctype", mqtt_set_encodings, NULL, OR_ALL,
                  "Set form encodings considered: URL MULTIPART ALL"),
        {NULL}
    };

static apr_pool_t *mqtt_pool = NULL;

/** allocation pool accessor#
 * use for data more persistent than a request
 * \return initial pool, different from the request pool
 */
apr_pool_t *mqtt_get_pool()
    {
    return mqtt_pool;
    }

/** allocation pool setter called once in register_hooks
 * use for data more persistent than a request
 * \return initial pool, different from the request pool
 */
apr_pool_t *mqtt_set_pool(apr_pool_t *p)
    {
    mqtt_pool = p;
    return p;
    }

/*
    ==============================================================================
    Our directive handlers:
    ==============================================================================
*/

/** Handler for the "MQTTEnabled" directive, default on 
  * Example: MQTTEnabled     on
  */
const char *
mqtt_set_enabled(cmd_parms *cmd, void *cfg, const char *arg)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    if (!strcasecmp(arg, "on"))
        config->enabled = 1;
    else
        config->enabled = 0;
    return NULL;
    }

/* Handler for the "MQTTServer" directive, default localhost
 * Example: MQTTServer      localhost 
 */
const char *
mqtt_set_server(cmd_parms *cmd, void *cfg, const char *arg)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    config->mqtt_server = arg;
    return NULL;
    }

/* Handler for the "MQTTURI" directive. Expressions like $action
 * are interpolated from variables. Required - no default
 * Example: MQTTURI "bla/fasel/scep/$action"         
 */
const char *
mqtt_set_uri(cmd_parms *cmd, void *cfg, const char *arg)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    config->mqtt_uri = arg;
    return NULL;
    }

/* Handler for the "MQTTPort" directive, default is 1883
 * Example: MQTTPort 1833         
 */
const char *
mqtt_set_port(cmd_parms *cmd, void *cfg, const char *arg)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    config->mqtt_port = atoi(arg);
    return NULL;
    }

/* Handler for the "MQTTVariables" directive: List of allowed variables
 * Required- no default. Use - to allow all variables.
 * eg MQTTVariables Id Name Action 
 */
const char *
mqtt_set_variables(cmd_parms *cmd, void *cfg, const char *arg)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    DPRINTF("VARS %s\n", arg);
    if (!config->mqtt_var_table)
        {
        config->mqtt_var_table =
            apr_table_make(mqtt_get_pool(), MQTT_MAX_VARS);
        }
    apr_table_add(config->mqtt_var_table, arg, arg);

    return NULL;
    }

/* Handler for the "MQTTCheckVariable" directive: REs Var values must match
 * Default is not to check
 * Example  MQTTCheckVariable Action ^submit|receive$ 
 */
const char *
mqtt_set_variable_checks(cmd_parms *cmd, void *cfg, const char *arg1, const char *arg2)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    DPRINTF("CHECKS %s %s\n", arg1, arg2);
    if (!config->mqtt_var_re_table)
        {
        config->mqtt_var_re_table =
            apr_table_make(mqtt_get_pool(), MQTT_MAX_VARS);
        }
    apr_table_add(config->mqtt_var_re_table, arg1, arg2);

    return NULL;
    }

/* Handler for the "MQTTMethods" directive: GET POST ALL
 * Default is ALL
 * Example  MQTTMethods ALL 
 */
const char *
mqtt_set_methods(cmd_parms *cmd, void *cfg, const char *arg)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    DPRINTF("METHODS %s\n", arg);

    config->methods = ALLMethods;

    if (!strcasecmp(arg, "GET"))
        config->methods = GETMethod;

    if (!strcasecmp(arg, "POST"))
        config->methods = GETMethod;

    return NULL;
    }

/* Handler for the "MQTTEnctype" directive: URL MULTIPART ALL
 *  Default is ALL
 * Example MQTTEnctype ALL 
 */
const char *
mqtt_set_encodings(cmd_parms *cmd, void *cfg, const char *arg)
    {
    mqtt_config *config = (mqtt_config *)cfg;
    DPRINTF("ENC %s\n", arg);
    config->encodings = ALLEncodings;

    if (!strcasecmp(arg, "URL"))
        config->encodings = URLEncoding;

    if (!strcasecmp(arg, "MULTIPART"))
        config->encodings = MultiPartEncoding;

    return NULL;
    }
