#!/usr/bin/perl -wT

print "Access-Control-Allow-Origin: *\n";

if ($ENV{'REQUEST_METHOD'} eq "OPTIONS") {
    # CORS preflight
    print "Access-Control-Allow-Headers: *\n";
    print "\n";
} elsif ($ENV{'REQUEST_METHOD'} eq "POST") {
    print "Content-type: text/plain\n\n";
    if ($ENV{'CONTENT_LENGTH'}) {
        read(STDIN, $postData, $ENV{'CONTENT_LENGTH'}) || die "Could not get post data\n";
    } else {
        $postData = "";
    }

    @list = split(/&/, $ENV{'QUERY_STRING'});
    foreach $element (@list) {
        ($key, $value) = split(/=/, $element);
        $values{$key} = $value;
    }

    open FILE, $values{'path'} || die("Could not open file\n");
    seek FILE, $values{'start'}, 0;
    read FILE, $expectedData, $values{'length'};
    close(FILE);

    if ($postData eq $expectedData) {
        print "OK";
    } else {
        print "FAILED";
    }
} else {
    print "Content-type: text/plain\n\n";
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
}
