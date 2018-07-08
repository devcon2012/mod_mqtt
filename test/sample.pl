#!/usr/bin/perl -w

use strict ;
use Data::Dumper ;


# use msquitto_sub to listen to mqtt, return string data received
sub mqtt_listen
    {
    print STDERR "Listen ... " ;
    open(my $in_fh, "-|", "mosquitto_sub -C 1 -t mod_mqtt/# ")  
        or die "Can't start mosquitto_sub: $!";

    my @data = <$in_fh> ;
    close($in_fh) or die "Can't close mosquitto_sub: $!";

    print STDERR " got: " .join '', @data ;
    return \@data ;
    }

# construct a reply - hashref with headers + data
sub answer
    {
    my $in = shift ;

    my $reply = 
        {
        'content-type'  => 'text/ascii',
        '.data'         => join('', @$in) ,
        };
    print STDERR "\nReply: " . Dumper($reply) ;
    return $reply ;
    }


# send answer via mosquitto_pub
sub mqtt_reply
    {
    my $reply_data = shift ;

    # use -l to send stdin
    open(my $out_fh, "|-", "mosquitto_pub -m 'TST' -t mod_mqtt/reply ")   
        or die "Can't start mosquitto_pub: $!";

    close($out_fh) or die "Can't close mosquitto_pub: $!";

    }

while (1)
    {
    mqtt_reply(answer(mqtt_listen())) ;
    }

