#!/usr/bin/perl -w

use strict;
use Data::Dumper;
use LWP::UserAgent ();

my $ua = LWP::UserAgent->new;
$ua->timeout(10);
$ua->env_proxy;

#my $response = $ua->get('http://search.cpan.org/');

#if ($response->is_success) {
#    print $response->decoded_content;  # or whatever
#}
#else {
#    die $response->status_line;
#}
my $url = "http://127.0.0.1/mqtt/sensors";

print "URL: $url \n";

my ($good, $bad) = (0, 0);

my $c = shift || 1 ;
my $n = $c ;
my $start = time ;
my $last = "";
my $lasterr = "";
   do {
    #foreach my $pars (qw(  sensorid=13&query=temperature )) 
    foreach my $pars (qw( sensorid=12&query=humidity sensorid=13&query=temperature sensorid=14&query=voltage )) 
        {
        #print "PARS: $pars\n";
        my $response = $ua->get("$url?$pars");

        if ( $response->is_success ) 
            {
            $good++ ;
            $last =  $response->decoded_content; 
            #print "Got: " . $response->decoded_content . "\n";    # or whatever
            }
        else 
            {
            $bad++ ;
            $lasterr = $response->status_line ;
            #print STDERR "" . $response->status_line . "\n";
            }
        }
    } while(--$c) ;

my $t = time - $start ;

print "Last successful response: '$last'\n";
print "Last error response: '$lasterr'\n";
print "Loops:$n, Good: $good, bad: $bad, time: $t [s] \n";
