#!/usr/bin/perl -wT
use utf8;
use Encode 'encode';

print "Content-type: text/plain; charset=utf-8\n\n";
print encode("UTF-8", "Проверка");
