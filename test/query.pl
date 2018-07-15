#!/usr/bin/perl -w

use strict ;
use Data::Dumper ;
use LWP::Simple;

print "OK:\n";
my $contents = get("http://127.0.0.1//mqtt/sensors?sensorid=13&query=temperature");

print $contents; 

#print "BAD:\n";
#$contents = get("http://127.0.0.1//mqtt/scep?axtion=illegal");

#print $contents;
