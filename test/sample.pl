#!/usr/bin/perl -w

use strict ;
use Data::Dumper ;
use File::Temp qw/ tempfile tempdir /;

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

    my $data = "bla dasle"; # join "", @$in;
    my $reply = << "END_REPLY";
        {
        "content-type": "text/ascii",
        ".data": "$data" 
        }
END_REPLY

    my $ts = localtime ;
    #print STDERR "\n$ts Reply: " . Dumper($reply) ;
    print STDERR "\n$ts Reply: \n";
    return $reply ;
    }


# send answer via mosquitto_pub
sub mqtt_reply
    {
    my $reply_data = shift ;
    my ($fh, $filename) = tempfile( 'MQTTREPLLXXXXXX', DIR => '/var/tmp');
        {
        local $/;
        print $fh $reply_data;
        close $fh ;
        }

    # use -l to send stdin
    open(my $out_fh, "|-", "mosquitto_pub -f $filename -t '$topic' ")   
        or die "Can't start mosquitto_pub: $!";

    my $ts = localtime ;
    print STDERR "\n$ts Reply sent.\n" ;

    close($out_fh) or die "Can't close mosquitto_pub: $!";
    unlink $filename or die "Can't unlink tempfile $filename: $!";
    }

while (1)
    {
    mqtt_reply(answer(mqtt_listen())) ;
    }

