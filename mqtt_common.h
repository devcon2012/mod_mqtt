/*
 * mqtt publish / subscribe common functions
 */

#ifndef _CLIENT_CONFIG_H
#define _CLIENT_CONFIG_H

#include <stdio.h>
#include <mosquitto.h>
#include "apr.h"
#include "apr_tables.h"

#ifdef DEBUG
#define DPRINTF(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

#define MESSAGE_COUNT 100000L
#define MESSAGE_SIZE 1024L

/* pub_client.c modes */
#define MSGMODE_NONE 0
#define MSGMODE_CMD 1
#define MSGMODE_STDIN_LINE 2
#define MSGMODE_STDIN_FILE 3
#define MSGMODE_FILE 4
#define MSGMODE_NULL 5

#define CLIENT_PUB 1
#define CLIENT_SUB 2

#define STATUS_CONNECTING 0
#define STATUS_CONNACK_RECVD 1
#define STATUS_WAITING 2
#define STATUS_DISCONNECTING 3


struct mosq_config
    {
    char *id;
    char *id_prefix;
    int protocol_version;
    int keepalive;
    char *host;
    int port;
    int qos;
    bool retain;
    int pub_mode;	 /* pub */
    char *file_input; /* pub */
    const char *message;	/* pub */
    long msglen;	  /* pub */
    char *topic;	  /* pub */
    int mode ;
    int status ;
    int connected;
    int last_mid;
    int last_mid_sent;
    int disconnect_sent;
    int mid_sent ;
    char *bind_address;
#ifdef WITH_SRV
    bool use_srv;
#endif
    bool debug;
    bool quiet;
    unsigned int max_inflight;
    char *username;
    char *password;
    char *will_topic;
    char *will_payload;
    long will_payloadlen;
    int will_qos;
    bool will_retain;
#ifdef WITH_TLS
    char *cafile;
    char *capath;
    char *certfile;
    char *keyfile;
    char *ciphers;
    bool insecure;
    char *tls_version;
#ifdef WITH_TLS_PSK
    char *psk;
    char *psk_identity;
#endif
#endif
    bool clean_session;   /* sub */
    char **topics;		  /* sub */
    int topic_count;	  /* sub */
    bool no_retain;		  /* sub */
    char **filter_outs;   /* sub */
    int filter_out_count; /* sub */
    bool verbose;		  /* sub */
    bool eol;			  /* sub */
    int msg_count;		  /* sub */
#ifdef WITH_SOCKS
    char *socks5_host;
    int socks5_port;
    char *socks5_username;
    char *socks5_password;
#endif
    apr_pool_t *pool;
    };

int mosquitto__parse_socks_url ( struct mosq_config *cfg, char *url );
int client_config_line_proc ( struct mosq_config *cfg, int pub_or_sub, int argc, char *argv[] );

int client_config_basic (apr_pool_t *pool,  struct mosq_config *cfg, const char * msg, int msglen);
int client_config_pub (struct mosq_config *cfg, const char * mqtt_server, int mqtt_port, const char * topic);
int client_config_sub (struct mosq_config *cfg, const char * mqtt_server, int mqtt_port, const char * topic);

int client_config_load (apr_pool_t *pool, struct mosq_config *config, int pub_or_sub, int argc, char *argv[] );

void client_config_cleanup ( struct mosq_config *cfg );
int client_opts_set ( struct mosquitto *mosq, struct mosq_config *cfg );
int client_id_generate ( struct mosq_config *cfg, const char *id_base );
int client_connect ( struct mosquitto *mosq, struct mosq_config *cfg );

int  mqtt_pub(apr_pool_t *pool, const char * mqtt_server, int mqtt_port, const char * topic, const char * msg, int msglen);
int  mqtt_sub(apr_pool_t *pool, const char * mqtt_server, int mqtt_port, const char * topic, char ** response, int * responselen);

#endif
