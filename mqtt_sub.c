/*
 * mqtt subscribe to response
 */


#include <assert.h>
#include <errno.h>
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

bool process_messages = true;
int msg_count = 0;
 

 /** This is called when a message is received from the broker.
    * \param mosq object
    * \param obj config object for this request
    * \param message message received
    */

void my_sub_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	struct mosq_config *cfg;
	int i;
	bool res;

	if (process_messages == false)
		return;

	assert(obj);
	cfg = (struct mosq_config *)obj;

	if (message->retain && cfg->no_retain)
		return;
	if (cfg->filter_outs)
	{
		for (i = 0; i < cfg->filter_out_count; i++)
		{
			mosquitto_topic_matches_sub(cfg->filter_outs[i], message->topic, &res);
			if (res)
				return;
		}
	}

	if (cfg->verbose)
	{
		if (message->payloadlen)
		{
			printf("%s ", message->topic);
			fwrite(message->payload, 1, message->payloadlen, stdout);
			if (cfg->eol)
			{
				printf("\n");
			}
		}
		else
		{
			if (cfg->eol)
			{
				printf("%s (null)\n", message->topic);
			}
		}
		fflush(stdout);
	}
	else
	{
		if (message->payloadlen)
		{
			fwrite(message->payload, 1, message->payloadlen, stdout);
			if (cfg->eol)
			{
				printf("\n");
			}
			fflush(stdout);
		}
	}
	if (cfg->msg_count > 0)
	{
		msg_count++;
		if (cfg->msg_count == msg_count)
		{
			process_messages = false;
			mosquitto_disconnect(mosq);
		}
	}
}

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

void my_sub_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	int i;
	struct mosq_config *cfg;

	assert(obj);
	cfg = (struct mosq_config *)obj;

	if (!result)
	{
		for (i = 0; i < cfg->topic_count; i++)
		{
			mosquitto_subscribe(mosq, NULL, cfg->topics[i], cfg->qos);
		}
	}
	else
	{
		if (result && !cfg->quiet)
		{
			fprintf(stderr, "%s\n", mosquitto_connack_string(result));
		}
	}
}



/** This is called when the broker responds to a subscription request.
    * \param mosq object
    * \param obj config object for this request
    * \param result from connect operation
	* \param mid message id
	* \param qos_count	the number of granted subscriptions (size of granted_qos).
	* \param granted_qos	an array of integers indicating the granted QoS for each of the subscriptions.
    */

void my_sub_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	struct mosq_config *cfg;

	assert(obj);
	cfg = (struct mosq_config *)obj;

	if (!cfg->quiet)
		printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++)
	{
		if (!cfg->quiet)
			printf(", %d", granted_qos[i]);
	}
	if (!cfg->quiet)
		printf("\n");
}

/**  This should be used if you want event logging information from the client library.
 * \param mosq	the mosquitto instance making the callback.
 * \param obj	the user data provided in mosquitto_new
 * \param level	the log message level from the values: MOSQ_LOG_INFO MOSQ_LOG_NOTICE MOSQ_LOG_WARNING MOSQ_LOG_ERR MOSQ_LOG_DEBUG
 * \param str	the message string.
 */
void my_sub_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str)
{
	printf("%s\n", str);
}

/**  single-shot subscribe to one message
 * \param pool request memory pool
 * \param mqtt_server server to use
 * \param mqtt_port server port to use
 * \param topic topic
 * \param response response message 
 * \param responselen response size
 * \return MOSQ_ERR_SUCCESS or ...
 */

int  mqtt_sub(apr_pool_t *pool, const char * mqtt_server, int mqtt_port, const char * topic, char ** response, int * responselen)
	{
    struct mosq_config cfg;
    struct mosquitto *mosq = NULL;
    int rc;

    DPRINTF("sub %s %d %s: \n", mqtt_server, mqtt_port, topic) ;
    
    rc = client_config_basic (pool, &cfg, NULL, 0);
    if ( rc != MOSQ_ERR_SUCCESS )
        return rc ;

    rc = client_config_sub (&cfg,  mqtt_server,  mqtt_port, topic) ;
    if ( rc != MOSQ_ERR_SUCCESS )
        return rc ;

    DPRINTF("cfg %d: \n", rc) ;

	mosquitto_lib_init();

	if (client_id_generate(&cfg, "mosqsub"))
	{
		return 1;
	}

    DPRINTF("cfg id %d: \n", rc) ;

	mosq = mosquitto_new(cfg.id, cfg.clean_session, &cfg);
	if (!mosq)
	{
		switch (errno)
		{
		case ENOMEM:
			if (!cfg.quiet)
				fprintf(stderr, "Error: Out of memory.\n");
			break;
		case EINVAL:
			if (!cfg.quiet)
				fprintf(stderr, "Error: Invalid id and/or clean_session.\n");
			break;
		}
		mosquitto_lib_cleanup();
		return 1;
	}

	if (client_opts_set(mosq, &cfg))
	{
		return 1;
	}
    DPRINTF("cb set: \n" ) ;

	if (cfg.debug)
	{
		mosquitto_log_callback_set(mosq, my_sub_log_callback);
		mosquitto_subscribe_callback_set(mosq, my_sub_subscribe_callback);
	}
	mosquitto_connect_callback_set(mosq, my_sub_connect_callback);
	mosquitto_message_callback_set(mosq, my_sub_message_callback);

	rc = client_connect(mosq, &cfg);
	if (rc)
		return rc;

	rc = mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	if (cfg.msg_count > 0 && rc == MOSQ_ERR_NO_CONN)
	{
		rc = 0;
	}

	if (rc)
	{
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
	}
	
	return rc;
}
