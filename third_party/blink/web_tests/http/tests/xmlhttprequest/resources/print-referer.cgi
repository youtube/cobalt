#!/usr/bin/perl -wT

use CGI qw(:standard);
my $cgi = new CGI;

print "Content-type: text/plain\n\n"; 
if ($cgi->referer) {
  print $cgi->referer;
} else {
  print "NO REFERER";
}
