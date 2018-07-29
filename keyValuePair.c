/*
 * keyValuePair : hash operations + similar stuff
 *
 * Klaus Ramstöck
 *
 */

#include <stdio.h>

#include "apache2/http_request.h"
#include "keyValuePair.h"
#include "mod_mqtt.h"
#include <jansson.h>

/** read urlencoded parameters from request and store as key-value pairs.
  * \param r the http request we process
  * \return  parameters from the url
  */

keyValuePair *readUrlArgs ( request_rec *r )
    {
    int i = 0, n = 0;
    char *buffer;
    keyValuePair *kvp;

    DPRINTF ( "--> readUrlArgs\n" );

    if ( !r->args )
        return NULL;

    buffer = apr_pcalloc ( r->pool, strlen ( r->args ) );
    strcpy ( buffer, r->args );

    for ( char *c = buffer; ( c = strchr ( c, '&' ) ); c++ )
        i++;

    kvp = apr_pcalloc ( r->pool, sizeof ( keyValuePair ) * ( MQTT_MAX_VARS + 2 ) );

    for ( char *c = buffer; c; )
        {
        char *k = c;
        char *v = strchr ( c, '=' );

        if ( v )
            {
            * ( v++ ) = 0;
            c = v;
            }
        else
            {
            v = apr_pcalloc ( r->pool, 1 );
            }

        kvp[n].key = k;
        kvp[n].value = v;

        c = strchr ( c, '&' );

        if ( c )
            {
            * ( c++ ) = 0;
            }

        n++;
        }

    DPRINTF ( "<-- readUrlArgs got %d\n", n );
    return kvp;
    }

/** read parameters from post request and add to key-value pairs in other.
  * \param r the http request we process
  * \param other NULL or kv-pairs obtained from the url
  * \return post parameters added
  */

keyValuePair *readPost ( request_rec *r, keyValuePair *other )
    {
    apr_array_header_t *pairs = NULL;
    apr_off_t len;
    apr_size_t size;
    int res;
    int i = 0;
    char *buffer;
    keyValuePair *kvp;

    res = ap_parse_form_data ( r, NULL, &pairs, -1, HUGE_STRING_LEN );

    if ( res != OK || !pairs )
        return NULL; /* Return NULL if we failed or if there are is no POST data */

    DPRINTF ( "--> strt with i= %d\n", i );

    if ( other )
        {
        while ( other[i].key )
            {
            i++;
            }
        }
    DPRINTF ( "--> cont with i= %d\n", i );

    kvp = ( other ? other : apr_pcalloc ( r->pool, sizeof ( keyValuePair ) * ( pairs->nelts + 1 ) ) );

    while ( pairs && !apr_is_empty_array ( pairs ) )
        {
        ap_form_pair_t *pair = ( ap_form_pair_t * ) apr_array_pop ( pairs );
        apr_brigade_length ( pair->value, 1, &len );
        size = ( apr_size_t ) len;
        buffer = apr_palloc ( r->pool, size + 1 );
        apr_brigade_flatten ( pair->value, buffer, &size );
        buffer[len] = 0;
        kvp[i].key = xstrdup ( r->pool, pair->name );
        kvp[i].value = buffer;
        i++;
        }
    DPRINTF ( "--> end with i= %d\n", i );

    return kvp;
    }

/** get a value for a key in a kvp
  * \param kvp key-value par datastructured filled with parameter data
  * \param key key/parameter name we search the value for
  * \return key values
  */
const char *keyValue ( keyValuePair *kvp, const char *key )
    {
    for ( int i = 0; kvp[i].key; i++ )
        {
        if ( kvp[i].key )
            {
            if ( strcmp ( kvp[i].key, key ) == 0 )
                {
                DPRINTF ( "--> found %s: %s\n", key, kvp[i].value );
                return kvp[i].value;
                }
            }
        }

    DPRINTF ( "--> not found  %s\n", key );
    return NULL;
    }

/** replace parts of a string with s.th else
  * \param p        allocation pool
  * \param orig     input string
  * \param rep      search data
  * \param with     replacement
  * \return string with replacements done
  */
const char *str_replace ( apr_pool_t *p, const char *orig, const char *rep, const char *with )
    {
    char *result;  // the return string
    const char *ins;     // the next insert point
    char *tmp;     // varies
    int len_rep;   // length of rep (the string to remove)
    int len_with;  // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;     // number of replacements

    // sanity checks and initialization
    if ( !orig || !rep )
        return NULL;

    len_rep = strlen ( rep );

    if ( len_rep == 0 )
        return NULL; // empty rep causes infinite loop during count

    if ( !with )
        with = "";

    len_with = strlen ( with );

    // count the number of replacements needed
    ins = orig;

    for ( count = 0; (tmp = strstr ( ins, rep )); ++count )
        {
        ins = tmp + len_rep;
        }

    DPRINTF ( "--> subst %d times %s with %s in %s\n", count, rep, with, orig );

    tmp = result = apr_pcalloc ( p, strlen ( orig ) + ( len_with - len_rep ) * count + 1 );

    if ( !result )
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while ( count-- )
        {
        ins = strstr ( orig, rep );
        len_front = ins - orig;
        tmp = strncpy ( tmp, orig, len_front ) + len_front;
        tmp = strcpy ( tmp, with ) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
        }

    strcpy ( tmp, orig );
    DPRINTF ( "--> yields %s\n", result );
    return result;
    }

/** replace keys referenced as $key in a target string
  * \param p    allocation pool
  * \param kvp  query parameters
  * \param key  key to replace
  * \param tgt  original string with key references a la $key
  * \return string with keys replaced by their values
  */
const char *keySubst ( apr_pool_t *p, keyValuePair *kvp, const char *key, const char *tgt )
    {
    DPRINTF ( "--> subst %s in %s\n", key, tgt );

    const char *val = keyValue ( kvp, key );

    if ( !val )
        return tgt;

    char *repl = apr_pcalloc ( p, strlen ( key ) + 1 );

    if ( !repl )
        return tgt;

    repl[0] = '$';
    strcpy ( repl + 1, key );
    const char *ret = str_replace ( p, tgt, repl, val );
    return ret;
    }

/** replace all keys from a key-value pair in a string by their values
  * \param p    allocation pool
  * \param kvp  query parameters
  * \param tgt  original string with key references a la $key
  * \return string with keys replaced by their values
  */
const char * kvSubst (apr_pool_t *p, keyValuePair *kvp, const char *tgt )
    {
    DPRINTF ( "--> kvSubst %ld\n", (long int) tgt );

    const char * b = xstrdup(p, tgt);

    for ( int i = 0; kvp[i].key; i++ )
        {
        if ( kvp[i].key )
            {
            const char * rep = keySubst(p, kvp, kvp[i].key, b) ;
            b =  xstrdup(p, rep);
            }
        }
    return b;
    }

/** duplicate a str as apr_pstrdup seems to coredump
  * \param p    allocation pool
  * \param src  original string
  * \return duplicate of src
  */
char * xstrdup(apr_pool_t *p, const char *src)
    {
    int nlen = strlen(src);
    char * dup = apr_pcalloc(p, nlen+1) ;
    if(dup)
        strcpy(dup, src);
    return dup;
    }

/** convert a keyValuePair datastr to json
  * \param p    allocation pool
  * \param vars vars to convert
  * \return json string data
  */
const char * kv2json(apr_pool_t *p, keyValuePair *vars)
    {
    DPRINTF ( "--> kv2json %ld\n", (long int) vars ) ;

    json_t *json = json_object() ;

    for ( int i = 0; vars[i].key; i++ )
        {
        if ( vars[i].key )
            {
            json_t *jv = json_string( vars[i].value );
            if(jv)
                {
                int jErr = json_object_set_new(json, vars[i].key, jv);
                if ( jErr )
                    fprintf(stderr, "json error 2 for %s: %s\n", vars[i].key, vars[i].value );
                }
            else
                fprintf(stderr, "json error 1 for %s: %s\n", vars[i].key, vars[i].value );
            }
        }

    size_t flags = JSON_SORT_KEYS;
    char *buf1 = json_dumps(json, flags);

    char *buf = xstrdup(p, buf1);

    free(buf1) ;
    json_decref(json);

    int size = strlen(buf) ;
    if ( size<0) size++ ;
    DPRINTF ( "--> kv2json %d bytes:  %s\n", size, buf );
    return buf ;
    }

/** convert a keyValuePair datastr to json
  * \param p    allocation pool
  * \param vars vars to convert
  * \return json string data
  */
keyValuePair * json2kv(apr_pool_t *p, const char *jsonbuf)
    {
    DPRINTF ( "--> json2kv %s\n", jsonbuf );
    keyValuePair *ret = apr_pcalloc ( p, sizeof ( keyValuePair ) * ( MQTT_MAX_VARS + 2 ) );

    size_t flags=0;
    json_error_t error ;
    json_t *json = json_loads(jsonbuf, flags, &error) ;
    if ( ! json )
        {
        fprintf(stderr, "json error %s\n", error.text) ;
        return NULL;
        }
    int i=0;
    const char *key;
    json_t *value;
    json_object_foreach(json, key, value) 
        {
        ret[i].key      = xstrdup(p, key);
        ret[i].value    = xstrdup(p, json_string_value(value));
        DPRINTF ( "--> json2kv %d %s %s\n", i, ret[i].key, ret[i].value );
        i++;
        if ( i>MQTT_MAX_VARS )
            {
            fprintf(stderr, "json2kv: More vars than expected");
            return ret ;
            }
        }

    json_decref(json);

    return ret ;
    }
