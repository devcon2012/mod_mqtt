#!/usr/bin/perl -w

use strict ;
use Data::Dumper ;

our $topic = "sensor/13/temperature" ;

# use msquitto_sub to listen to mqtt, return string data received
sub mqtt_listen
    {
    my $ts = localtime ;
    print STDERR "$ts : Subscribe $topic ... " ;
    open(my $in_fh, "-|", "mosquitto_sub -C 1 -t '$topic' ")  
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
    my $ts = localtime ;
    #print STDERR "\n$ts Reply: " . Dumper($reply) ;
    print STDERR "\n$ts Reply: \n";
    return $reply ;
    }


# send answer via mosquitto_pub
sub mqtt_reply
    {
    my $reply_data = shift ;

    # use -l to send stdin
    open(my $out_fh, "|-", "mosquitto_pub -m 'TST' -t '$topic' ")   
        or die "Can't start mosquitto_pub: $!";

    my $ts = localtime ;
    print STDERR "\n$ts Reply sent.\n" ;

    close($out_fh) or die "Can't close mosquitto_pub: $!";
    }

while (1)
    {
    mqtt_reply(answer(mqtt_listen())) ;
    }

