package Release::Patcher::Config;

use strict;

use MozBuild::Util qw(GetBuildIDFromFTP);
use Bootstrap::Util qw(GetBouncerToPatcherPlatformMap GetBouncerPlatforms GetBuildbotToFTPPlatformMap GetFTPToBuildbotPlatformMap GetEqualPlatforms);

use base qw(Exporter);
our @EXPORT_OK = qw(GetProductDetails GetReleaseBlock BumpFilePath BumpURL);

sub GetProductDetails {
    my %args = @_;
    my $product = $args{'product'};
    my $appVersion = $args{'appVersion'};
    my $updateType = $args{'updateType'};

    my $endOfUrl = '/releasenotes/';
    if ($product eq 'seamonkey') {
        $endOfUrl = '/';
    }
    elsif ($updateType eq 'major') {
        $endOfUrl = '/details/index.html';
    }

    if ($product eq 'seamonkey') {
        return 'http://www.seamonkey-project.org/releases/' . $product .
               $appVersion . $endOfUrl;
    }
    elsif ($product eq 'thunderbird') {
        return 'https://www.mozillamessaging.com/%locale%/' . $product . '/' .
               $appVersion . $endOfUrl;
    }
    else {
        return 'https://www.mozilla.com/%locale%/' . $product . '/' .
               $appVersion . $endOfUrl;
    }
}

sub GetReleaseBlock {
    my %args = @_;
    my $version = $args{'version'};
    my $appVersion = $args{'appVersion'};
    my $prettyVersion = $args{'prettyVersion'};
    my $product = $args{'product'};
    my $buildStr = $args{'buildstr'};
    my $stagingServer = $args{'stagingServer'};
    my $localeInfo = $args{'localeInfo'};
    my $platforms = $args{'platforms'};
    my $schema = $args{'schema'};
    my @marChannelIds = @{$args{"marChannelIds"}};

    my $releaseBlock = {};
    $releaseBlock->{'schema'} = $schema;
    $releaseBlock->{'version'} = $appVersion;
    $releaseBlock->{'extension-version'} = $appVersion;
    $releaseBlock->{'prettyVersion'} = $prettyVersion;

    my $candidateDir = '/pub/mozilla.org/' . $product . '/nightly/' .
      $version . '-candidates/' . $buildStr;

    $releaseBlock->{'platforms'} = {};
    my %platformFTPMap = GetBuildbotToFTPPlatformMap();
    foreach my $os (@$platforms){
        my $buildID = GetBuildIDFromFTP(os => $os,
                                        releaseDir => $candidateDir,
                                        stagingServer => $stagingServer);
        if (exists($platformFTPMap{$os})){
            my $ftp_platform = $platformFTPMap{$os};
            $releaseBlock->{'platforms'}->{$ftp_platform} = $buildID;
        } else {
            die("ASSERT: GetReleaseBlock(): unknown OS $os");
        }
    }
    
    $releaseBlock->{'locales'} = join(' ', sort (keys(%{$localeInfo})));
    
    $releaseBlock->{'completemarurl'} = 'http://' . $stagingServer .
      '/pub/mozilla.org/' . $product . '/nightly/' . $version . '-candidates/' .
      $buildStr . '/update/%platform%/%locale%/' . $product . '-' . $version .
      '.complete.mar';
    $releaseBlock->{'checksumsurl'} = 'http://' . $stagingServer .
      '/pub/mozilla.org/' . $product . '/nightly/' . $version . '-candidates/' .
      $buildStr . '/%platform%/%locale%/' . $product . '-' . $version .
      '.checksums';
    if (@marChannelIds) {
        $releaseBlock->{"mar-channel-ids"} = join(",", @marChannelIds);
    }

    $releaseBlock->{'exceptions'} = {};

    my %platformMap = GetBouncerToPatcherPlatformMap();
    my %FTPplatformMap = GetFTPToBuildbotPlatformMap();
    foreach my $locale (keys(%{$localeInfo})) {
        my $allPlatformsHash = {};
        foreach my $platform (GetBouncerPlatforms()) {
            $allPlatformsHash->{$platform} = 1;
        }

        foreach my $localeSupportedPlatform (@{$localeInfo->{$locale}}) {
            die 'ASSERT: GetReleaseBlock(): platform in locale, but not in' .
             ' all locales? Invalid platform?' if
             (!exists($allPlatformsHash->{$localeSupportedPlatform}));

            delete $allPlatformsHash->{$localeSupportedPlatform};
        }

        my @supportedPatcherPlatforms = ();
        foreach my $platform (@{$localeInfo->{$locale}}) {
            if (exists $FTPplatformMap{$platformMap{$platform}} &&
                grep($FTPplatformMap{$platformMap{$platform}} eq $_, @{$platforms})){
                    push(@supportedPatcherPlatforms, $platformMap{$platform});
            }
            # Get platforms not mentioned in shipped-locales
            my $equal_platforms = GetEqualPlatforms($platform);
            if ($equal_platforms){
                foreach my $equal_platform (@{$equal_platforms}){
                    push(@supportedPatcherPlatforms, $platformMap{$equal_platform})
                        if grep($FTPplatformMap{$platformMap{$equal_platform}} eq $_, @{$platforms});
                }
            }
        }
        if (keys(%{$allPlatformsHash}) > 0) {
            $releaseBlock->{'exceptions'}->{$locale} =
             join(', ', sort(@supportedPatcherPlatforms));
        }
    }

    return $releaseBlock;
}

sub BumpFilePath {
    my %args = @_;
    my $oldFilePath = $args{'oldFilePath'};
    my $product = $args{'product'};
    my $marName = $args{'marName'};
    my $oldMarName = $args{'oldMarName'};
    my $version = $args{'version'};
    my $oldVersion = $args{'oldVersion'};

    # strip out everything up to and including 'buildN/'
    my $newPath = $oldFilePath;
    $newPath =~ s/.*\/build\d+\///;
    # We need to handle partials and complete MARs differently
    if ($newPath =~ m/\.partial\.mar$/) {
        $newPath =~ s/($oldMarName|$marName)-(.+?)\.partial
                     /$marName-$oldVersion-$version.partial/x
                     or die("ASSERT: BumpFilePath() - Could not bump path: " .
                            "'$oldFilePath' from '$oldVersion' to '$version'");
    } elsif ($newPath =~ m/\.complete\.mar$/) {
        $newPath =~ s/($oldMarName|$marName)-(.+?)\.complete
                     /$marName-$version.complete/x
                     or die("ASSERT: BumpFilePath() - Could not bump path: " .
                            "'$oldFilePath' from '$oldVersion' to '$version'");
    } else {
        die("ASSERT: BumpFilePath() - Unknown file type for '$oldFilePath'");
    }

    return $newPath;
}

sub BumpURL {
    my %args = @_;
    my $oldURL = $args{'oldURL'};
    my $version = $args{'version'};
    my $oldVersion = $args{'oldVersion'};

    my $newURL = $oldURL;
    $newURL =~ s/$oldVersion/$version/;

    return $newURL;
}

1;
