#!/usr/bin/perl -wT

print "Content-type: text/css\n"; 
print "Cache-control: no-store\n\n";
sleep(1);
print "#pass { display: inline }\n";
print "#fail { display: none }\n";

