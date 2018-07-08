/*
Copyright (c) 2009-2018 Roger Light <roger@atchoo.org>

All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License v1.0
and Eclipse Distribution License v1.0 which accompany this distribution.
 
The Eclipse Public License is available at
   http://www.eclipse.org/legal/epl-v10.html
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.
 
Contributors:
   Roger Light - initial implementation and documentation.
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

void my_sub_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str)
{
	printf("%s\n", str);
}

int mqtt_sub(int argc, char *argv[])
{
	struct mosq_config cfg;
	struct mosquitto *mosq = NULL;
	int rc;

	rc = client_config_load(&cfg, CLIENT_SUB, argc, argv);
	if (rc)
	{
		client_config_cleanup(&cfg);
		fprintf(stderr, "\nmosquitto malconfigured\n");
		return 1;
	}

	mosquitto_lib_init();

	if (client_id_generate(&cfg, "mosqsub"))
	{
		return 1;
	}

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
