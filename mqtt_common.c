/*
 * mqtt publish / subscribe common functions
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <process.h>
#include <winsock2.h>
#define snprintf sprintf_s
#endif

#include <mosquitto.h>
#include "mqtt_common.h"
#include "keyValuePair.h"

#define _DEBUG

/** init a config to default values
  * \param pool - request memory pool
  * \param cfg config to initialize
  */
void init_config ( apr_pool_t *pool, struct mosq_config *cfg )
    {
    memset ( cfg, 0, sizeof ( *cfg ) );
    cfg->pool = pool;
    cfg->port = 1883;
    cfg->max_inflight = 20;
    cfg->keepalive = 60;
    cfg->clean_session = true;
    cfg->eol = true;
    cfg->protocol_version = MQTT_PROTOCOL_V31;
    }

/** free memory consumed - noop, we use the request pool
  * \param cfg config to initialize
  */
void client_config_cleanup ( struct mosq_config *cfg )
    {
        /* No free- all mem is from pool and will be freed as request is serviced */
    }

/** free memory consumed - noop, we use the request pool
  * \param pool - request memory pool
  * \param cfg config to initialize
  * \param message buffer
  * \param message buffer size
  * \return MOSQ_ERR_SUCCESS
  */
int client_config_basic (apr_pool_t *pool,  struct mosq_config *cfg, const char * msg, int msglen)
    {
    init_config ( pool, cfg );
    cfg -> message = msg ;
    cfg -> msglen = msglen ;
    return MOSQ_ERR_SUCCESS ;
    }

/** set config for publishing
  * \param cfg config to initialize
  * \param mqtt_server buffer
  * \param mqtt_port buffer size
  * \param topic mqtt topic
  * \return MOSQ_ERR_SUCCESS
  */
int client_config_pub (struct mosq_config * cfg, const char * mqtt_server, int mqtt_port, 
                        const char * topic)
{
    apr_pool_t *pool = cfg -> pool ;

    cfg->port = mqtt_port;

    if ( cfg->port < 1 || cfg->port > 65535 )
        {
        fprintf ( stderr, "Error: Invalid port given: %d\n", cfg->port );
        return 1;
        }
    cfg->msg_count = 1;

    if ( cfg->msg_count < 1 )
        {
        fprintf ( stderr, "Error: Invalid message count \"%d\".\n\n", cfg->msg_count );
        return 1;
        }

#ifdef _DEBUG
    cfg->debug = true;
#endif

    cfg->bind_address = cfg->host = xstrdup (pool, mqtt_server );
    cfg->pub_mode = MSGMODE_CMD;

    if ( mosquitto_pub_topic_check ( topic ) == MOSQ_ERR_INVAL )
        {
        fprintf ( stderr, "Error: Invalid publish topic '%s', does it contain '+' or '#'?\n", topic );
        return 1;
        }
    cfg->topic_count = 1 ;
    cfg->topics = apr_pcalloc ( pool, sizeof ( char * ) );
    cfg->topic = cfg->topics[0] = xstrdup ( pool, topic );

    return MOSQ_ERR_SUCCESS;
    }

/** set config for subscription
  * \param cfg config to initialize
  * \param mqtt_server buffer
  * \param mqtt_port buffer size
  * \param topic mqtt topic
  * \return MOSQ_ERR_SUCCESS
  */
int client_config_sub (struct mosq_config * cfg, const char * mqtt_server, int mqtt_port, 
                        const char * topic)
{
    apr_pool_t *pool = cfg -> pool ;

    cfg->port = mqtt_port;

    if ( cfg->port < 1 || cfg->port > 65535 )
        {
        fprintf ( stderr, "Error: Invalid port given: %d\n", cfg->port );
        return 1;
        }
    /* cfg->bind_address = xstrdup ( pool, mqtt_server ); */
    cfg->msg_count = 1;

    if ( cfg->msg_count < 1 )
        {
        fprintf ( stderr, "Error: Invalid message count \"%d\".\n\n", cfg->msg_count );
        return 1;
        }
#ifdef _DEBUG
    cfg->debug = true;
#endif

    cfg->host = xstrdup (pool, mqtt_server );
    cfg->pub_mode = MSGMODE_CMD;
    cfg->mode = MSGMODE_CMD;

    if ( mosquitto_pub_topic_check ( topic ) == MOSQ_ERR_INVAL )
        {
        fprintf ( stderr, "Error: Invalid publish topic '%s', does it contain '+' or '#'?\n", topic );
        return 1;
        }
    cfg->topic_count = 1 ;
    cfg->topics = apr_pcalloc ( pool, sizeof ( char * ) );
    cfg->topics[0] = xstrdup ( pool, topic );

    return MOSQ_ERR_SUCCESS;
    }

/** load configuration
  * \param pool request memory pool
  * \param cfg config to initialize
  * \param pub_or_sub 
  * \param argc 
  * \param argv 
  * \return MOSQ_ERR_SUCCESS
  */
int client_config_load ( apr_pool_t *pool, struct mosq_config *cfg, int pub_or_sub, int argc, char *argv[] )
    {
    int rc;
    FILE *fptr;
    char line[1024];
    int count;
    char *loc = NULL;
    int len;
    char *args[3];

#ifndef WIN32
    char *env;
#else
    char env[1024];
#endif
    args[0] = NULL;

    init_config (pool,  cfg );

    /* Default config file */
#ifndef WIN32
    env = getenv ( "XDG_CONFIG_HOME" );

    if ( env )
        {
        len = strlen ( env ) + strlen ( "/mosquitto_pub" ) + 1;
        loc = apr_pcalloc ( pool, len );

        if ( !loc )
            {
            fprintf ( stderr, "Error: Out of memory.\n" );
            return 1;
            }

        if ( pub_or_sub == CLIENT_PUB )
            {
            snprintf ( loc, len, "%s/mosquitto_pub", env );
            }
        else
            {
            snprintf ( loc, len, "%s/mosquitto_sub", env );
            }

        loc[len - 1] = '\0';
        }
    else
        {
        env = getenv ( "HOME" );

        if ( env )
            {
            len = strlen ( env ) + strlen ( "/.config/mosquitto_pub" ) + 1;
            loc = apr_pcalloc ( pool, len );

            if ( !loc )
                {
                fprintf ( stderr, "Error: Out of memory.\n" );
                return 1;
                }

            if ( pub_or_sub == CLIENT_PUB )
                {
                snprintf ( loc, len, "%s/.config/mosquitto_pub", env );
                }
            else
                {
                snprintf ( loc, len, "%s/.config/mosquitto_sub", env );
                }

            loc[len - 1] = '\0';
            }
        else
            {
            fprintf ( stderr, "Warning: Unable to locate configuration directory, default config not loaded.\n" );
            }
        }

#else
    rc = GetEnvironmentVariable ( "USERPROFILE", env, 1024 );

    if ( rc > 0 && rc < 1024 )
        {
        len = strlen ( env ) + strlen ( "\\mosquitto_pub.conf" ) + 1;
        loc = apr_pcalloc ( pool, len );

        if ( !loc )
            {
            fprintf ( stderr, "Error: Out of memory.\n" );
            return 1;
            }

        if ( pub_or_sub == CLIENT_PUB )
            {
            snprintf ( loc, len, "%s\\mosquitto_pub.conf", env );
            }
        else
            {
            snprintf ( loc, len, "%s\\mosquitto_sub.conf", env );
            }

        loc[len - 1] = '\0';
        }
    else
        {
        fprintf ( stderr, "Warning: Unable to locate configuration directory, default config not loaded.\n" );
        }

#endif

    if ( loc )
        {
        fptr = fopen ( loc, "rt" );

        if ( fptr )
            {
            while ( fgets ( line, 1024, fptr ) )
                {
                if ( line[0] == '#' )
                    continue; /* Comments */

                while ( line[strlen ( line ) - 1] == 10 || line[strlen ( line ) - 1] == 13 )
                    {
                    line[strlen ( line ) - 1] = 0;
                    }

                /* All offset by one "args" here, because real argc/argv has
                 * program name as the first entry. */
                args[1] = strtok ( line, " " );

                if ( args[1] )
                    {
                    args[2] = strtok ( NULL, " " );

                    if ( args[2] )
                        {
                        count = 3;
                        }
                    else
                        {
                        count = 2;
                        }

                    rc = client_config_line_proc ( cfg, pub_or_sub, count, args );

                    if ( rc )
                        {
                        fclose ( fptr );
                        return rc;
                        }
                    }
                }

            fclose ( fptr );
            }

        }

    /* Deal with real argc/argv */
    rc = client_config_line_proc ( cfg, pub_or_sub, argc, argv );

    if ( rc )
        return rc;

    if ( cfg->will_payload && !cfg->will_topic )
        {
        fprintf ( stderr, "Error: Will payload given, but no will topic given.\n" );
        return 1;
        }

    if ( cfg->will_retain && !cfg->will_topic )
        {
        fprintf ( stderr, "Error: Will retain given, but no will topic given.\n" );
        return 1;
        }

    if ( cfg->password && !cfg->username )
        {
        if ( !cfg->quiet )
            fprintf ( stderr, "Warning: Not using password since username not set.\n" );
        }

#ifdef WITH_TLS

    if ( ( cfg->certfile && !cfg->keyfile ) || ( cfg->keyfile && !cfg->certfile ) )
        {
        fprintf ( stderr, "Error: Both certfile and keyfile must be provided if one of them is.\n" );
        return 1;
        }

#endif
#ifdef WITH_TLS_PSK

    if ( ( cfg->cafile || cfg->capath ) && cfg->psk )
        {
        if ( !cfg->quiet )
            fprintf ( stderr, "Error: Only one of --psk or --cafile/--capath may be used at once.\n" );

        return 1;
        }

    if ( cfg->psk && !cfg->psk_identity )
        {
        if ( !cfg->quiet )
            fprintf ( stderr, "Error: --psk-identity required if --psk used.\n" );

        return 1;
        }

#endif

    if ( pub_or_sub == CLIENT_SUB )
        {
        if ( cfg->clean_session == false && ( cfg->id_prefix || !cfg->id ) )
            {
            if ( !cfg->quiet )
                fprintf ( stderr, "Error: You must provide a client id if you are using the -c option.\n" );

            return 1;
            }

        if ( cfg->topic_count == 0 )
            {
            if ( !cfg->quiet )
                fprintf ( stderr, "Error: You must specify a topic to subscribe to.\n" );

            return 1;
            }
        }

    if ( !cfg->host )
        {
        cfg->host = "localhost";
        }

    return MOSQ_ERR_SUCCESS;
    }

/** Process a tokenised single line from a file or set of real argc/argv 
  * \param cfg config to initialize
  * \param pub_or_sub 
  * \param argc 
  * \param argv 
  * \return MOSQ_ERR_SUCCESS
  */

int client_config_line_proc ( struct mosq_config *cfg, int pub_or_sub, int argc, char *argv[] )
    {
    return MOSQ_ERR_SUCCESS ;
    }

/** set options from config
  * \param mosq object to set options
  * \param cfg config with options
  * \return MOSQ_ERR_SUCCESS
  */
int client_opts_set ( struct mosquitto *mosq, struct mosq_config *cfg )
    {
    if ( cfg->will_topic && mosquitto_will_set ( mosq, cfg->will_topic,
            cfg->will_payloadlen, cfg->will_payload, cfg->will_qos,
            cfg->will_retain ) )
        {

        if ( !cfg->quiet )
            fprintf ( stderr, "Error: Problem setting will.\n" );

        mosquitto_lib_cleanup();
        return 1;
        }

    if ( cfg->username && mosquitto_username_pw_set ( mosq, cfg->username, cfg->password ) )
        {
        if ( !cfg->quiet )
            fprintf ( stderr, "Error: Problem setting username and password.\n" );

        mosquitto_lib_cleanup();
        return 1;
        }

#ifdef WITH_TLS

    if ( ( cfg->cafile || cfg->capath ) && mosquitto_tls_set ( mosq, cfg->cafile, cfg->capath, cfg->certfile, cfg->keyfile, NULL ) )
        {

        if ( !cfg->quiet )
            fprintf ( stderr, "Error: Problem setting TLS options.\n" );

        mosquitto_lib_cleanup();
        return 1;
        }

    if ( cfg->insecure && mosquitto_tls_insecure_set ( mosq, true ) )
        {
        if ( !cfg->quiet )
            fprintf ( stderr, "Error: Problem setting TLS insecure option.\n" );

        mosquitto_lib_cleanup();
        return 1;
        }

#ifdef WITH_TLS_PSK

    if ( cfg->psk && mosquitto_tls_psk_set ( mosq, cfg->psk, cfg->psk_identity, NULL ) )
        {
        if ( !cfg->quiet )
            fprintf ( stderr, "Error: Problem setting TLS-PSK options.\n" );

        mosquitto_lib_cleanup();
        return 1;
        }

#endif

    if ( ( cfg->tls_version || cfg->ciphers ) && mosquitto_tls_opts_set ( mosq, 1, cfg->tls_version, cfg->ciphers ) )
        {
        if ( !cfg->quiet )
            fprintf ( stderr, "Error: Problem setting TLS options.\n" );

        mosquitto_lib_cleanup();
        return 1;
        }

#endif
    mosquitto_max_inflight_messages_set ( mosq, cfg->max_inflight );
#ifdef WITH_SOCKS

    if ( cfg->socks5_host )
        {
        rc = mosquitto_socks5_set ( mosq, cfg->socks5_host, cfg->socks5_port, cfg->socks5_username, cfg->socks5_password );

        if ( rc )
            {
            mosquitto_lib_cleanup();
            return rc;
            }
        }

#endif
    mosquitto_opts_set ( mosq, MOSQ_OPT_PROTOCOL_VERSION, & ( cfg->protocol_version ) );
    return MOSQ_ERR_SUCCESS;
    }

/** generate a msg id
  * \param cfg config with options
  * \param id_base 
  * \return MOSQ_ERR_SUCCESS or ..
  */

int client_id_generate ( struct mosq_config *cfg, const char *id_base )
    {
    int len;
    char hostname[256];

    if ( cfg->id_prefix )
        {
        cfg->id = malloc ( strlen ( cfg->id_prefix ) + 10 );

        if ( !cfg->id )
            {
            if ( !cfg->quiet )
                fprintf ( stderr, "Error: Out of memory.\n" );

            mosquitto_lib_cleanup();
            return 1;
            }

        snprintf ( cfg->id, strlen ( cfg->id_prefix ) + 10, "%s%d", cfg->id_prefix, getpid() );
        }
    else if ( !cfg->id )
        {
        hostname[0] = '\0';
        gethostname ( hostname, 256 );
        hostname[255] = '\0';
        len = strlen ( id_base ) + strlen ( "|-" ) + 6 + strlen ( hostname );
        cfg->id = malloc ( len );

        if ( !cfg->id )
            {
            if ( !cfg->quiet )
                fprintf ( stderr, "Error: Out of memory.\n" );

            mosquitto_lib_cleanup();
            return 1;
            }

        snprintf ( cfg->id, len, "%s|%d-%s", id_base, getpid(), hostname );

        if ( strlen ( cfg->id ) > MOSQ_MQTT_ID_MAX_LENGTH )
            {
            /* Enforce maximum client id length of 23 characters */
            cfg->id[MOSQ_MQTT_ID_MAX_LENGTH] = '\0';
            }
        }

    return MOSQ_ERR_SUCCESS;
    }

/** connect our client to the server
  * \param mosq 
  * \param cfg config with options
  * \return MOSQ_ERR_SUCCESS or ..
  */

int client_connect ( struct mosquitto *mosq, struct mosq_config *cfg )
    {
    char err[1024];
    int rc;

    DPRINTF( "client_connect: %s %d %d %s\n", cfg->host, cfg->port, cfg->keepalive, cfg->bind_address);
#ifdef WITH_SRV
    DPRINTF("client_connect SRV %d\n", cfg->use_srv ) ;

    if ( cfg->use_srv )
        {
        rc = mosquitto_connect_srv ( mosq, cfg->host, cfg->keepalive, cfg->bind_address );
        }
    else
        {
        rc = mosquitto_connect_bind ( mosq, cfg->host, cfg->port, cfg->keepalive, cfg->bind_address );
        }

#else
    DPRINTF("client_connect_bind\n") ;
    rc = mosquitto_connect_bind ( mosq, cfg->host, cfg->port, cfg->keepalive, cfg->bind_address );
#endif

    DPRINTF("client_connect %d:\n", rc) ;

    if ( rc > 0 )
        {
        if ( !cfg->quiet )
            {
            if ( rc == MOSQ_ERR_ERRNO )
                {
#ifndef WIN32
                char * b = strerror_r ( errno, err, 1024 );
                if ( !b )
                    fprintf ( stderr, "unknown error %d\n", errno );
                else
#else
                FormatMessage ( FORMAT_MESSAGE_FROM_SYSTEM, NULL, errno, 0, ( LPTSTR ) &err, 1024, NULL );
#endif
                fprintf ( stderr, "Error: %s\n", err );
                }
            else
                {
                fprintf ( stderr, "Unable to connect (%s).\n", mosquitto_strerror ( rc ) );
                }
            }

        mosquitto_lib_cleanup();
        return rc;
        }
     cfg->connected = 1;
    return MOSQ_ERR_SUCCESS;
    }

/* Convert %25 -> %, %3a, %3A -> :, %40 -> @ */
static int mosquitto__urldecode ( char *str )
    {
    int i, j;
    int len;

    if ( !str )
        return 0;

    if ( !strchr ( str, '%' ) )
        return 0;

    len = strlen ( str );

    for ( i = 0; i < len; i++ )
        {
        if ( str[i] == '%' )
            {
            if ( i + 2 >= len )
                {
                return 1;
                }

            if ( str[i + 1] == '2' && str[i + 2] == '5' )
                {
                str[i] = '%';
                len -= 2;

                for ( j = i + 1; j < len; j++ )
                    {
                    str[j] = str[j + 2];
                    }

                str[j] = '\0';
                }
            else if ( str[i + 1] == '3' && ( str[i + 2] == 'A' || str[i + 2] == 'a' ) )
                {
                str[i] = ':';
                len -= 2;

                for ( j = i + 1; j < len; j++ )
                    {
                    str[j] = str[j + 2];
                    }

                str[j] = '\0';
                }
            else if ( str[i + 1] == '4' && str[i + 2] == '0' )
                {
                str[i] = ':';
                len -= 2;

                for ( j = i + 1; j < len; j++ )
                    {
                    str[j] = str[j + 2];
                    }

                str[j] = '\0';
                }
            else
                {
                return 1;
                }
            }
        }

    return 0;
    }


