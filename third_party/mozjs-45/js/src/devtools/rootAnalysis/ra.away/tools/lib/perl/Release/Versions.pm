package Release::Versions;

use strict;

use base qw(Exporter);
our @EXPORT_OK = qw(GetPrettyVersion);

sub GetPrettyVersion {
    my %args = @_;
    my $version = $args{'version'};
    my $prettyVersion = $args{'version'};
    my $product = $args{'product'};

    $prettyVersion =~ s/a([0-9]+)$/ Alpha $1/;
    if ($product eq 'firefox' && $version ge '5') {
        $prettyVersion =~ s/b([0-9]+)$/ Beta/;
    } else {
        $prettyVersion =~ s/b([0-9]+)$/ Beta $1/;
    }
    $prettyVersion =~ s/rc([0-9]+)$/ RC $1/;

    return $prettyVersion;
}
