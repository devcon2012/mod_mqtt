#
# requires mosquitto (https://mosquitto.org) and jansson (http://www.digip.org/jansson)
#
#
#

mod_mqtt.la: keyValuePair.c  keyValuePair.h  mod_mqtt.c  mod_mqtt_conf.c  mod_mqtt.h  \
		mod_mqtt_work.c  mqtt_common.c  mqtt_common.h  mqtt_pub.c  mqtt_sub.c
	apxs  -D NODEBUG -a -l jansson -l mosquitto -I /usr/include/apr-1 -l apr-1 -c mod_mqtt.c mod_mqtt_conf.c mod_mqtt_work.c keyValuePair.c \
			mqtt_common.c mqtt_pub.c  mqtt_sub.c

clean:
	rm -f mod_mqtt.la *.lo  *.slo

install: mod_mqtt.la
	apxs -i -a mod_mqtt.la
	cp mqtt.conf /etc/apache2/conf.d/
	rcapache2 restart
	systemctl status -l apache2.service

doc:
	(cd doc ; doxygen)

log:
	tail -f /var/log/apache2/error_log 

.PHONY: doc log