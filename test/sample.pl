#!/usr/bin/perl -w

use strict ;
use Data::Dumper ;
use File::Temp qw/ tempfile tempdir /;
use JSON;

our $topic = "sensor/13/temperature" ;
our $count = 1;

our $in_fh ;

# use msquitto_sub to listen to mqtt, return string data received
sub mqtt_listen
    {
    my $ts = localtime ;
    my $ptopic = "sensor/#";

    print STDERR "$ts : Subscribe $ptopic ... " ;
    if ( ! defined $in_fh )
        {
        open($in_fh, "-|", "mosquitto_sub  -t '$ptopic' ")  
            or die "Can't #start mosquitto_sub: $!";
        }

    do 
        {
        my $line = <$in_fh> ;
        die "subscribe at eof"  if ( ! defined $line ) ;
        print STDERR " got from $ptopic: $line" ;
        my $data = decode_json $line;
        return $data ;
        }
    while(1);
    }

# construct a reply - hashref with headers + data
sub answer
    {
    my $in = shift ;
    my $query = $in -> {query};
    my $sensorid = $in -> {sensorid};
    my $rnd = int rand 1000;
    my $reply = << "END_REPLY";
        {
        "content-type": "text/ascii",
        ".data": "$query of sensor $sensorid is $rnd" 
        }
END_REPLY

    my $ts = localtime ;
    #print STDERR "\n$ts Reply: " . Dumper($reply) ;
    return $reply ;
    }


# send answer via mosquitto_pub
sub mqtt_reply
    {
    my $reply_data = shift ;
    return if ( ! $reply_data ) ;
    my ($fh, $filename) = tempfile( 'MQTTREPLLXXXXXX', DIR => '/var/tmp');
        {
        local $/;
        print $fh $reply_data;
        close $fh ;
        }

    # use -l to send stdin
    my $stopic = "sensorvalues/sub" ;
    open(my $out_fh, "|-", "mosquitto_pub -f $filename -t '$stopic' ")   
        or die "Can't start mosquitto_pub: $!";

    my $ts = localtime ;
    print STDERR "\n$ts Reply $count sent to $stopic.\n" ;
    $count++;

    close($out_fh) or die "Can't close mosquitto_pub: $!";
    unlink $filename or die "Can't unlink tempfile $filename: $!";
    }

while (1)
    {
    mqtt_reply(answer(mqtt_listen())) ;
    }

