/*
 * mqtt publish data from http req
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

 /** This is called when the broker sends a CONNACK message in response to a connection.
    * mosq	the mosquitto instance making the callback.
    * obj	the user data provided in mosquitto_new
    * rc	the return code of the connection response, one of:
    *   0 - success
    *   1 - connection refused (unacceptable protocol version)
    * 2 - connection refused (identifier rejected)
    * 3 - connection refused (broker unavailable)
    * 4-255 - reserved for future use
    * \param mosq object
    * \param obj config object for this request
    * \param result from connect operation
    */

void my_pub_connect_callback ( struct mosquitto *mosq, void *obj, int result )
    {
    struct mosq_config *cb_obj = ( struct mosq_config *) obj ;
    int rc = MOSQ_ERR_SUCCESS;

    DPRINTF("my_pub_connect_callback %d: \n", result) ;

    if ( !result )
        {
        switch ( cb_obj-> pub_mode )
            {
            case MSGMODE_CMD:
            case MSGMODE_FILE:
            case MSGMODE_STDIN_FILE:
                rc = mosquitto_publish ( mosq, & (cb_obj -> mid_sent),
                                            cb_obj -> topic, 
                                            cb_obj -> msglen, 
                                            cb_obj -> message, 
                                            cb_obj -> qos, 
                                            cb_obj -> retain );
                break;

            case MSGMODE_NULL:
                rc = mosquitto_publish ( mosq, & (cb_obj -> mid_sent), 
                                            cb_obj -> topic, 
                                            0, 
                                            NULL, 
                                            cb_obj -> qos, 
                                            cb_obj -> retain );
                break;

            case MSGMODE_STDIN_LINE:
                cb_obj ->status = STATUS_CONNACK_RECVD;
                break;
            }

        DPRINTF("my_pub_connect_callback rc %d: \n", rc) ;

        if ( rc )
            {
            if ( !cb_obj ->quiet )
                {
                switch ( rc )
                    {
                    case MOSQ_ERR_INVAL:
                        fprintf ( stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n" );
                        break;

                    case MOSQ_ERR_NOMEM:
                        fprintf ( stderr, "Error: Out of memory when trying to publish message.\n" );
                        break;

                    case MOSQ_ERR_NO_CONN:
                        fprintf ( stderr, "Error: Client not connected when trying to publish.\n" );
                        break;

                    case MOSQ_ERR_PROTOCOL:
                        fprintf ( stderr, "Error: Protocol error when communicating with broker.\n" );
                        break;

                    case MOSQ_ERR_PAYLOAD_SIZE:
                        fprintf ( stderr, "Error: Message payload is too large.\n" );
                        break;
                    }
                }

            mosquitto_disconnect ( mosq );
            }
        }
    else
        {
        if ( result && !cb_obj->quiet )
            {
            fprintf ( stderr, "%s\n", mosquitto_connack_string ( result ) );
            }
        }
    }

/**  This is called when the broker has received the DISCONNECT command and has disconnected the client.
  * \param mosq	the mosquitto instance making the callback.
  * \param obj config for this request
  * \param rc	integer value indicating the reason for the disconnect. 
  *         A value of 0 means the client has called mosquitto_disconnect. 
  *         Any other value indicates that the disconnect is unexpected.
  */
void my_pub_disconnect_callback ( struct mosquitto *mosq, void *obj, int rc )
    {
    struct mosq_config *cb_obj = ( struct mosq_config *) obj ;
    cb_obj -> connected = false;

    DPRINTF("my_pub_disconnect_callback %d: \n", rc) ;
    }

/** This is called when a message initiated with mosquitto_publish has been sent to the broker successfully.
 * @par mosq	the mosquitto instance making the callback.
 * @par obj	the user data provided in mosquitto_new
 * @par mid	the message id of the sent message.
 */
void my_pub_publish_callback ( struct mosquitto *mosq, void *obj, int mid )
    {
    struct mosq_config *cb_obj = ( struct mosq_config *) obj ;
    cb_obj-> last_mid_sent = mid;

    DPRINTF("my_pub_publish_callback %d: \n", mid) ;

    if ( cb_obj->disconnect_sent == false )
        {
        mosquitto_disconnect ( mosq );
        cb_obj->disconnect_sent = true;
        }
    }

    

/**  This should be used if you want event logging information from the client library.
 * \param mosq	the mosquitto instance making the callback.
 * \param obj	the user data provided in mosquitto_new
 * \param level	the log message level from the values: MOSQ_LOG_INFO MOSQ_LOG_NOTICE MOSQ_LOG_WARNING MOSQ_LOG_ERR MOSQ_LOG_DEBUG
 * \param str	the message string.
 */
void my_pub_log_callback ( struct mosquitto *mosq, void *obj, int level, const char *str )
    {
    /* struct mosq_config *cb_obj = ( struct mosq_cb_obj *) obj ; */
    }

/**  single-shot publish one message
 * \param pool request memory pool
 * \param mqtt_server server to use
 * \param mqtt_port server port to use
 * \param topic topic
 * \param msg message 
 * \param msglen message size
 * \return MOSQ_ERR_SUCCESS or ...
 */
int  mqtt_pub(apr_pool_t *pool, const char * mqtt_server, int mqtt_port, const char * topic, const char * msg, int msglen)
    {
    struct mosq_config cfg;
    struct mosquitto *mosq = NULL;
    int rc;

    DPRINTF("pub %s %d %s, %s %d: \n", mqtt_server, mqtt_port, topic, msg, msglen) ;
    
    rc = client_config_basic (pool, &cfg, msg, msglen);
    if ( rc != MOSQ_ERR_SUCCESS )
        return rc ;

    rc = client_config_pub (&cfg,  mqtt_server,  mqtt_port, topic) ;
    if ( rc != MOSQ_ERR_SUCCESS )
        return rc ;

    DPRINTF("cfg %d: \n", rc) ;

    mosquitto_lib_init();
    DPRINTF("cfg init \n");

    if ( client_id_generate ( &cfg, "mosqpub" ) )
        {
        return 1;
        }

    DPRINTF("cfg id %d: \n", rc) ;

    mosq = mosquitto_new ( cfg.id, true, &cfg );
    DPRINTF("cfg new %ld: \n", (long int) mosq) ;

    if ( !mosq )
        {
        switch ( errno )
            {
            case ENOMEM:
                if ( !cfg.quiet )
                    fprintf ( stderr, "Error: Out of memory.\n" );

                break;

            case EINVAL:
                if ( !cfg.quiet )
                    fprintf ( stderr, "Error: Invalid id.\n" );

                break;
            }

        mosquitto_lib_cleanup();
        return 1;
        }

    if ( cfg.debug )
        {
        mosquitto_log_callback_set ( mosq, my_pub_log_callback );
        }

    mosquitto_connect_callback_set ( mosq, my_pub_connect_callback );
    mosquitto_disconnect_callback_set ( mosq, my_pub_disconnect_callback );
    mosquitto_publish_callback_set ( mosq, my_pub_publish_callback );

    DPRINTF("cb set: \n" ) ;

    if ( client_opts_set ( mosq, &cfg ) )
        {
        return 1;
        }

    DPRINTF("opts set\n" ) ;

    rc = client_connect ( mosq, &cfg );
    DPRINTF("connected: %d\n", rc ) ;

    if ( rc )
        return rc;

    do
        {
        rc = mosquitto_loop ( mosq, -1, 1 );
        }
    while ( rc == MOSQ_ERR_SUCCESS && cfg.connected );

    mosquitto_destroy ( mosq );
    mosquitto_lib_cleanup();

    if ( rc )
        {
        fprintf ( stderr, "Error: %s\n", mosquitto_strerror ( rc ) );
        }

    DPRINTF("pub finished %d\n", rc ) ;

    return rc;
    }
