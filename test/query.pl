#!/usr/bin/perl -w

use strict ;
use Data::Dumper ;
use LWP::Simple;

my $contents = get("http://127.0.0.1//mqtt/scep?action=submit");

print $contents; 