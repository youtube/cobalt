#!/usr/bin/perl -w
use strict;

use File::Path;
use File::Spec::Functions;
use File::Temp qw(tempdir);
use Getopt::Long;

use MozBuild::Util qw(GetBuildIDFromFTP);
use Bootstrap::Util qw(LoadLocaleManifest);

$|++;

my %config;

ProcessArgs();
if (defined $config{'run-tests'}) {
    RunUnitTests();
} else {
    BumpVerifyConfig();
}

sub ProcessArgs {
	GetOptions(
		\%config,
        "osname|o=s", "product|p=s", "brand|r=s", "old-version=s", "old-app-version=s",
        "old-long-version=s", "version|v=s", "app-version=s", "long-version=s",
        "build-number|n=s", "aus-server-url|a=s", "staging-server|s=s",
        "verify-config|c=s", "old-candidates-dir|d=s", "linux-extension|e=s",
        "shipped-locales|l=s", "pretty-candidates-dir", "major|m",
        "binary-name=s", "old-binary-name=s", "--test-older-partials",
        "old-shipped-locales=s", "channel=s", "help", "run-tests"
    );

    if ($config{'help'}) {
        print <<__USAGE__;
Usage: update-verify-bump.pl [options]
This script depends on the MozBuild::Util and Bootstrap::Util modules.
Options requiring arguments:
  --osname/-o             One of (linux, linux64, macosx, macosx64, win32).
  --product/-p            The product name (eg. firefox, thunderbird, seamonkey, etc.).
  --brand/-r              The brand name (eg. Firefox, Thunderbird, SeaMonkey, etc.)
                          If not specified, a first-letter-uppercased product name is assumed.
  --binary-name           Optinal binary prefix (eg. Firefox,
                          MozillaDeveloperPreview) for this version. Default value is set to
                          the brand name.
  --old-binary-name       Optional binary prefix (eg. Firefox,
                          MozillaDeveloperPreview) for the previous version. Default value
                          is set to the brand name.
  --old-version           The previous version of the product.
  --old-app-version       The previous 'app version of the product'.
                          If not passed, is assumed to be the same as
                          --old-version.
  --old-long-version      The previous long version of the product.
                          Eg. '3.1 Beta 1'. If not passed, is assumed to be the
                          same as --old-version.
  --version/-v            The current version of the product. Eg. 3.0.3, 3.1b1,
                          3.1rc2.
  --app-version           The current app version of the product. If not passed,
                          is assumed to be the same as --version. Usually the
                          same as version except for RC releases. Eg, when
                          version is 3.1rc2, appVersion is 3.1.
  --long-version          The current long version of the product. Only
                          necessary when --pretty-candidates-dir is passed and
                          is different than --version.
  --build-number/-n       The current build number of the product.
  --aus-server-url/-a     The URL to be used when constructing update URLs.
                          Eg. https://aus4.mozilla.org
  --staging-server/-s     The staging server that builds should be pulled from.
  --verify-config/-c      The path and filename of the config file to be bumped.
  --old-candidates-dir/-d The path (on the staging server) to the candidates
                          directory for the previous release.
                          Eg. /home/ftp/pub/firefox/nightly/3.1b1-candidates/build1
  --linux-extension/-e    The extension to use for Linux packages. Assumed to
                          be bz2 if not passed.
  --shipped-locales/-l    The path and filename to the shipped-locales file
                          for this release.
  --old-shipped-locales   The path and filename to the old-shipped-locales file
                          for this release.
  --channel               The channel to test on. Defaults to betatest.
Options without arguments:
  --pretty-candidates-dir When passed, the "to" field in the verify config will
                          be formatted the "pretty" way by using the long
                          version number on mac and win32, and storing files
                          in a deep directory format rather than encoding all
                          of the information into the filename. This flag does
                          not take an argument.
  --major/-m              Major update (MU) mode, which starts the file from
                          scratch rather than appending
  --test-older-partials   When passed, all old releases will be marked for
                          partial update testing. When not passed, only
                          oldVersion will be marked for partial update testing.
                          In major update mode this controls whether or not
                          oldVersion will make its partial update marked for
                          testing or not.
  --help                  This usage message. This flag takes no arguments.
  --run-tests             Run the (very basic) unit tests include with this
                          script. This flag takes no arguments.
__USAGE__

        exit(0);
    }

    my $error = 0;

    # Input validation. Not necessary if we're running tests.
    if (! defined $config{'run-tests'}) {
        for my $arg ('osname', 'product', 'old-version',
                     'version', 'build-number', 'aus-server-url',
                     'staging-server', 'old-candidates-dir') {
            if (! defined $config{$arg}) {
                print "$arg must be defined.\n";
                $error = 1;
            }
        }
        if (! defined $config{'verify-config'} or
                 ! -w $config{'verify-config'}) {
            print "verify config file must exist and be writable.\n";
            $error = 1;
        }
        if (! defined $config{'shipped-locales'} or
                 ! -e $config{'shipped-locales'}) {
            print "shipped locales file must exist.\n";
            $error = 1;
        }
        if (! defined $config{'old-shipped-locales'} or
                 ! -e $config{'old-shipped-locales'}) {
            print "old shipped locales file must exist.\n";
            $error = 1;
        }
        if (defined $config{'pretty-candidates-dir'} and
               ! defined $config{'long-version'}) {
            print "long-version must be defined when passing " .
                  "--pretty-candidates-dir";
            $error = 1;
        } 
        if ($error) {
            exit($error);
        }

        # set sane defaults
        if (! defined $config{'brand'}) {
            $config{'brand'} = ucfirst($config{'product'});
        }
        if (! defined $config{'binary-name'}) {
            $config{'binary-name'} = $config{'brand'};
        }
        if (! defined $config{'old-binary-name'}) {
            $config{'old-binary-name'} = $config{'brand'};
        }
        if (! defined $config{'old-app-version'}) {
            $config{'old-app-version'} = $config{'old-version'};
        }
        if (! defined $config{'old-long-version'}) {
            $config{'old-long-version'} = $config{'old-version'};
        }
        if (! defined $config{'app-version'}) {
            $config{'app-version'} = $config{'version'};
        }
        if (! defined $config{'long-version'}) {
            $config{'long-version'} = $config{'version'};
        }
        if (! defined $config{'linux-extension'}) {
            $config{'linux-extension'} = 'bz2';
        }
        if (! defined $config{'pretty-candidates-dir'}) {
            $config{'pretty-candidates-dir'} = 0;
        }
        if (! defined $config{'major'}) {
            $config{'major'} = 0;
        }
        if (! defined $config{'test-older-partials'}) {
            $config{'test-older-partials'} = 0;
        }
        if (! defined $config{'channel'}) {
            $config{'channel'} = 'betatest';
        }
    }
}

sub BumpVerifyConfig {
    my $this = shift;

    my $osname = $config{'osname'};
    my $product = $config{'product'};
    my $brand = $config{'brand'};
    my $binaryName = $config{'binary-name'};
    my $oldBinaryName = $config{'old-binary-name'};
    my $oldVersion = $config{'old-version'};
    my $oldAppVersion = $config{'old-app-version'};
    my $oldLongVersion = $config{'old-long-version'};
    my $version = $config{'version'};
    my $appVersion = $config{'app-version'};
    my $longVersion = $config{'long-version'};
    my $build = $config{'build-number'};
    my $ausServerUrl = $config{'aus-server-url'};
    my $stagingServer = $config{'staging-server'};
    my $configFile = $config{'verify-config'};
    my $candidatesDir = $config{'old-candidates-dir'};
    my $linuxExtension = $config{'linux-extension'};
    my $shippedLocales = $config{'shipped-locales'};
    my $oldShippedLocales = $config{'old-shipped-locales'};
    my $prettyCandidatesDir = $config{'pretty-candidates-dir'};
    my $majorMode = $config{'major'};
    my $testOlderPartials = $config{'test-older-partials'};
    my $channel = $config{'channel'};

    my $buildID = GetBuildIDFromFTP(os => $osname,
                                    releaseDir => $candidatesDir,
                                    stagingServer => $stagingServer);

    my $buildTarget = undef;
    my $platform = undef;
    my $releaseFile = undef;
    my $nightlyFile = undef;
    my $ftpOsname = undef;

    if ($osname eq 'linux') {
        $buildTarget = 'Linux_x86-gcc3';
        $platform = 'linux';
        $ftpOsname = 'linux-i686';
        $releaseFile = lc($oldBinaryName).'-'.$oldVersion.'.tar.'.$linuxExtension;
        if ($prettyCandidatesDir) {
            $nightlyFile = 'linux-i686/%locale%/'.lc($binaryName).'-'.$version.
             '.tar.'.$linuxExtension;
        } else {
            $nightlyFile = lc($binaryName).'-'.$appVersion.'.%locale%.linux-i686.tar.'.
             $linuxExtension;
        }
    } elsif ($osname eq 'linux64') {
        $buildTarget = 'Linux_x86_64-gcc3';
        $platform = 'linux';
        $ftpOsname = 'linux-x86_64';
        $releaseFile = lc($oldBinaryName).'-'.$oldVersion.'.tar.'.$linuxExtension;
        if ($prettyCandidatesDir) {
            $nightlyFile = 'linux-x86_64/%locale%/'.lc($binaryName).'-'.$version.
             '.tar.'.$linuxExtension;
        } else {
            $nightlyFile = lc($binaryName).'-'.$appVersion.'.%locale%.linux-x86_64.tar.'.
             $linuxExtension;
        }
    } elsif ($osname eq 'macosx') {
        # bug 630085 - we're only updating people running i386 from Fx3.5 and
        # Fx3.6 to Fx4. TODO: Needs to play nice with other apps. Will get
        # redone to fix bug 633124
        if ($majorMode and $product eq 'firefox' and $appVersion > '4.0') {
            $buildTarget = 'Darwin_x86-gcc3-u-ppc-i386';
        } else {
            $buildTarget = 'Darwin_Universal-gcc3';
        }
        $platform = 'osx';
        $ftpOsname = 'mac';
        $releaseFile = $oldBinaryName.' '.$oldLongVersion.'.dmg';
        if ($prettyCandidatesDir) {
            $nightlyFile = 'mac/%locale%/'.$binaryName.' '.$longVersion.
             '.dmg';
        } else {
            $nightlyFile = lc($binaryName).'-'.$appVersion.'.%locale%.mac.dmg';
        }
    } elsif ($osname eq 'macosx64') {
        $buildTarget = 'Darwin_x86_64-gcc3-u-i386-x86_64';
        $platform = 'osx';
        $ftpOsname = 'mac';
        $releaseFile = $oldBinaryName.' '.$oldLongVersion.'.dmg';
        if ($prettyCandidatesDir) {
            $nightlyFile = 'mac/%locale%/'.$binaryName.' '.$longVersion.
             '.dmg';
        } else {
            $nightlyFile = lc($binaryName).'-'.$appVersion.'.%locale%.mac.dmg';
        }
    } elsif ($osname eq 'win32') {
        $buildTarget = 'WINNT_x86-msvc';
        $platform = 'win32';
        $ftpOsname = 'win32';
        $releaseFile = $oldBinaryName.' Setup '.$oldLongVersion.'.exe';
        if ($prettyCandidatesDir) {
            $nightlyFile = 'win32/%locale%/'.$binaryName. ' Setup '.
             $longVersion.'.exe';
        } else {
            $nightlyFile = lc($binaryName).'-'.$appVersion.
             '.%locale%.win32.installer.exe';
        }
    } else {
        die("ASSERT: unknown OS $osname");
    }

    open(FILE, "< $configFile") or die ("Could not open file $configFile: $!");
    my @origFile = <FILE>;
    close(FILE) or die ("Could not close file $configFile: $!");

    my @strippedFile = ();
    # If the file is already bumped for this version, undo it, so we can do it
    # again.
    if ($origFile[0] =~ /$oldVersion\s/) {
            print "verifyConfig $configFile already bumped\n";
            print "removing previous config..\n";
            # remove top two lines; the comment and the version config
            splice(@origFile, 0, 2);
            @strippedFile = @origFile;
    # If the file isn't bumped for this version, we split out the current
    # "full" check into a small "full" and other "quick" checks. We check
    # for emptiness because this will be empty for new update verify configs.
    } elsif ($origFile[1] !~ /^\s*$/) {
        # convert existing full check to a full check for top N locales,
        # and quick check for everything else
        my $versionComment = $origFile[0];
        # Get rid of the newline because we'll be appending some text
        chomp($versionComment);
        my $line = $origFile[1];
        chomp($line);
        # Get rid of all locales except de, en-US, ru (if they exist) because
        # we can't afford to do full checks on everything.
        $line =~ s/locales="(.*?)( ?de ?)(.*?)( ?en-US ?)(.*?)( ?ru ?)(.*?)"/locales="$2$4$6"/;
        # Remove any extra whitespace
        $line =~ s/\s{2,}/ /g;
        $line =~ s/locales="\s*/locales="/;
        $line =~ s/(locales=".*?)\s*"/$1"/;
        # Full checks don't need this or anything after this.
        $line =~ s/ aus_server.*$//;
        # Also be sure to remove patch_types if we're not testing older partials
        if (! $testOlderPartials ) {
            $line =~ s/ patch_types=".*?"//;
        }
        push(@strippedFile, ($versionComment . " - full check\n"));
        push(@strippedFile, ($line . "\n"));

        $line = $origFile[1];
        chomp($line);
        # remove the locales we're already doing a full check on
        foreach my $l (("de", "en-US", "ru")) {
            $line =~ s/(locales=".*?)$l ?(.*?")/$1$2/;
        }
        $line =~ s/ from.*$//;
        if (! $testOlderPartials ) {
            $line =~ s/ patch_types=".*?"//;
        }
        # We may not have any locales left here, so don't add anything if
        # there's no locales to test!
        if ($line !~ m/locales="\s*"/) {
            push(@strippedFile, ($versionComment . " - quick check\n"));
            push(@strippedFile, ($line . "\n"));
        }
        
        # all the old release lines, assume they're OK
        for(my $i=2; $i < scalar(@origFile); $i++) {
            push(@strippedFile, ($origFile[$i]));
        }
    }

    my $localeManifest = {};
    if (not LoadLocaleManifest(localeHashRef => $localeManifest,
                               manifest => $shippedLocales)) {
        die "Could not load locale manifest";
    }

    my $oldLocaleManifest = {};
    if (not LoadLocaleManifest(localeHashRef => $oldLocaleManifest,
                               manifest => $oldShippedLocales)) {
        die "Could not load old locale manifest";
    }

    my @locales = ();
    foreach my $locale (keys(%{$localeManifest})) {
        foreach my $allowedPlatform (@{$localeManifest->{$locale}}) {
            if ($allowedPlatform eq $platform &&
                exists $oldLocaleManifest->{$locale} &&
                grep($platform eq $_, @{$oldLocaleManifest->{$locale}})){
                push(@locales, $locale);
            }
        }
    }

    # add data for latest release
    my $from = '/' . $product . '/releases/' . $oldVersion . '/' . $ftpOsname .
        '/%locale%/' . $releaseFile;
    if ( $majorMode ) {
        my $candidatesDirStripped = $candidatesDir;
        $candidatesDirStripped =~ s!/$!!; # Remove trailing slash
        $candidatesDirStripped =~ s!^/.*?/$product/!/$product/!; # Remove FTP prefix
        $from = $candidatesDirStripped . '/' . $ftpOsname . '/%locale%/' . $releaseFile;
    }
    my @data = ("# $oldVersion $osname\n",
                'release="' . $oldAppVersion . '" product="' . $brand . 
                '" platform="' .$buildTarget . '" build_id="' . $buildID . 
                '" locales="' . join(' ', sort(@locales)) . '" channel="' . 
                $channel . '"');
    # Bug 514040 changed the default behaviour of update verify to test only
    # the complete MAR. This is a bit of an abuse of what testOlderPartials
    # means, but we need a way to distinguish between major updates from
    # branches which need a partial the same as the complete, and those that
    # don't. (ASSUMPTION: ) The oldVersion for all minor releases recieves a
    # partial, so we must override patch_types here whenever $majorMode is
    # false.
    if ( ! $majorMode || $testOlderPartials ) {
        $data[1] .= ' patch_types="partial complete"'
    }
    $data[1] .= ' from="' . $from .
                '" aus_server="' . $ausServerUrl . '" ftp_server="' .
                $stagingServer . '/pub/mozilla.org" to="/' . 
                $product . '/nightly/' .  $version .  '-candidates/build' . 
                $build . '/' . $nightlyFile . '"' . "\n";

    open(FILE, "> $configFile") or die ("Could not open file $configFile: $!");
    print FILE @data;
    # write the data for the old releases unless we're doing a major update
    if ( ! $majorMode ) {
        print FILE @strippedFile;
    }
    close(FILE) or die ("Could not close file $configFile: $!");
}

sub RunUnitTests {
    # 3.0.x minor update test
    ExecuteTest(product => 'firefox',
                brand => 'Firefox',
                binaryName => 'Firefox',
                oldBinaryName => 'Firefox',
                osname => 'linux',
                platform => 'Linux_x86-gcc3',
                version => '3.0.3',
                longVersion => '3.0.3',
                build => '1',
                oldVersion => '3.0.2',
                oldLongVersion => '3.0.2',
                oldBuildid => '2008091618',
                oldFromBuild => '/firefox/releases/3.0.2/linux-i686/%locale%/firefox-3.0.2.tar.bz2',
                oldToBuild => '/firefox/nightly/3.0.3-candidates/build1/firefox-3.0.3.%locale%.linux-i686.tar.bz2',
                oldOldVersion => '3.0.1',
                oldOldLongVersion => '3.0.1',
                oldOldBuildid => '2008070206',
                oldOldFromBuild => '/firefox/releases/3.0.1/linux-i686/%locale%/firefox-3.0.1.tar.bz2',
                oldOldToBuild => '/firefox/nightly/3.0.2-candidates/build6/firefox-3.0.2.%locale%.linux-i686.tar.bz2',
                ausServer => 'https://aus4.mozilla.org',
                stagingServer => 'build.mozilla.org',
                oldCandidatesDir => '/update-bump-unit-tests/pub/mozilla.org/firefox/nightly/3.0.2-candidates/build6',
                prettyCandidatesDir => '0',
                linuxExtension => 'bz2',
                major => 0,
                testOlderPartials => 1
    );
    # 3.1b minor update test (pretty names)
    ExecuteTest(product => 'firefox',
                brand => 'Firefox',
                binaryName => 'Firefox',
                oldBinaryName => 'Firefox',
                osname => 'win32',
                platform => 'WINNT_x86-msvc',
                version => '3.1b2',
                longVersion => '3.1 Beta 2',
                build => '1',
                oldVersion => '3.1b1',
                oldLongVersion => '3.1 Beta 1',
                oldBuildid => '20081007144708',
                oldFromBuild => '/firefox/releases/3.1b1/win32/%locale%/Firefox Setup 3.1 Beta 1.exe',
                oldToBuild => '/firefox/nightly/3.1b2-candidates/build1/win32/%locale%/Firefox Setup 3.1 Beta 2.exe',
                oldOldVersion => '3.1a2',
                oldOldLongVersion => '3.1 Alpha 2',
                oldOldBuildid => '20080829082037',
                oldOldFromBuild => '/firefox/releases/shiretoko/alpha2/win32/en-US/Shiretoko Alpha 2 Setup.exe',
                oldOldToBuild => '/firefox/nightly/3.1b1-candidates/build2/win32/%locale%/Firefox 3.1 Beta 1 Setup.exe',
                ausServer => 'https://aus4.mozilla.org',
                stagingServer => 'build.mozilla.org',
                oldCandidatesDir => '/update-bump-unit-tests/pub/mozilla.org/firefox/nightly/3.1b1-candidates/build2',
                prettyCandidatesDir => '1',
                linuxExtension => 'bz2',
                major => 0,
                testOlderPartials => 1
    );
    ExecuteTest(product => 'firefox',
                brand => 'Firefox',
                binaryName => 'Firefox',
                oldBinaryName => 'MozillaDeveloperPreview',
                osname => 'win32',
                platform => 'WINNT_x86-msvc',
                version => '3.6b1',
                longVersion => '3.6 Beta 1',
                build => '3',
                oldVersion => '3.6a1',
                oldLongVersion => '3.6 Alpha 1',
                oldBuildid => '20090806165642',
                oldFromBuild => '/firefox/releases/3.6a1/win32/%locale%/MozillaDeveloperPreview Setup 3.6 Alpha 1.exe',
                oldToBuild => '/firefox/nightly/3.6b1-candidates/build3/win32/%locale%/Firefox Setup 3.6 Beta 1.exe',
                oldOldVersion => '3.5.7',
                oldOldLongVersion => '3.5.7',
                oldOldBuildid => '20091221164558',
                oldOldFromBuild => '/firefox/nightly/3.5.7-candidates/build1/win32/%locale%/Firefox Setup 3.5.7.exe',
                oldOldToBuild => '/firefox/nightly/3.6a1-candidates/build1/win32/%locale%/MozillaDeveloperPreview Setup 3.6 Aplha 1.exe',
                ausServer => 'https://aus4.mozilla.org',
                stagingServer => 'build.mozilla.org',
                oldCandidatesDir => '/update-bump-unit-tests/pub/mozilla.org/firefox/nightly/3.6a1-candidates/build1',
                prettyCandidatesDir => '1',
                linuxExtension => 'bz2',
                major => 0,
                testOlderPartials => 0
    );
    # major update test
    ExecuteTest(product => 'firefox',
                brand => 'Firefox',
                binaryName => 'Firefox',
                oldBinaryName => 'Firefox',
                osname => 'win32',
                platform => 'WINNT_x86-msvc',
                version => '3.6b4',
                longVersion => '3.6 Beta 4',
                build => '1',
                oldVersion => '3.5.5',
                oldLongVersion => '3.5.5',
                oldBuildid => '20091102152451',
                oldFromBuild => '/firefox/nightly/3.5.5-candidates/build1/win32/%locale%/Firefox Setup 3.5.5.exe',
                oldToBuild => '/firefox/nightly/3.6b4-candidates/build1/win32/%locale%/Firefox Setup 3.6 Beta 4.exe',
                oldOldVersion => '',
                oldOldLongVersion => '',
                oldOldBuildid => '',
                oldOldFromBuild => '',
                oldOldToBuild => '',
                ausServer => 'https://aus4.mozilla.org',
                stagingServer => 'build.mozilla.org',
                oldCandidatesDir => '/update-bump-unit-tests/pub/mozilla.org/firefox/nightly/3.5.5-candidates/build1',
                prettyCandidatesDir => '1',
                linuxExtension => 'bz2',
                major => 1,
                testOlderPartials => 0
    );
    ExecuteTest(product => 'firefox',
                brand => 'Firefox',
                binaryName => 'Firefox',
                oldBinaryName => 'Firefox',
                osname => 'win32',
                platform => 'WINNT_x86-msvc',
                version => '3.6b4',
                longVersion => '3.6 Beta 4',
                build => '1',
                oldVersion => '3.5.5',
                oldLongVersion => '3.5.5',
                oldBuildid => '20091102152451',
                oldFromBuild => '/firefox/nightly/3.5.5-candidates/build1/win32/%locale%/Firefox Setup 3.5.5.exe',
                oldToBuild => '/firefox/nightly/3.6b4-candidates/build1/win32/%locale%/Firefox Setup 3.6 Beta 4.exe',
                oldOldVersion => '',
                oldOldLongVersion => '',
                oldOldBuildid => '',
                oldOldFromBuild => '',
                oldOldToBuild => '',
                ausServer => 'https://aus4.mozilla.org',
                stagingServer => 'build.mozilla.org',
                oldCandidatesDir => '/update-bump-unit-tests/pub/mozilla.org/firefox/nightly/3.5.5-candidates/build1',
                prettyCandidatesDir => '1',
                linuxExtension => 'bz2',
                major => 1,
                testOlderPartials => 1
    );
}

sub ExecuteTest {
    my %args = @_;
    my $product = $args{'product'};
    my $brand = $args{'brand'};
    my $binaryName = $args{'binaryName'};
    my $oldBinaryName = $args{'oldBinaryName'};
    my $osname = $args{'osname'};
    my $platform = $args{'platform'};
    my $version = $args{'version'};
    my $longVersion = $args{'longVersion'};
    my $build = $args{'build'};
    my $oldVersion = $args{'oldVersion'};
    my $oldLongVersion = $args{'oldLongVersion'};
    my $oldBuildid = $args{'oldBuildid'};
    my $oldFromBuild = $args{'oldFromBuild'};
    my $oldToBuild = $args{'oldToBuild'};
    my $oldOldVersion = $args{'oldOldVersion'};
    my $oldOldLongVersion = $args{'oldOldLongVersion'};
    my $oldOldBuildid = $args{'oldOldBuildid'};
    my $oldOldFromBuild = $args{'oldOldFromBuild'};
    my $oldOldToBuild = $args{'oldOldToBuild'};
    my $ausServer = $args{'ausServer'};
    my $stagingServer = $args{'stagingServer'};
    my $oldCandidatesDir = $args{'oldCandidatesDir'};
    my $prettyCandidatesDir = $args{'prettyCandidatesDir'};
    my $linuxExtension = $args{'linuxExtension'};
    my $majorMode = $args{'major'};
    my $testOlderPartials = $args{'testOlderPartials'};

    my $workdir = tempdir(CLEANUP => 0);
    my $bumpedConfig = catfile($workdir, 'bumped-update-verify.cfg');
    my $referenceConfig = catfile($workdir, 'reference-update-verify.cfg');
    my $shippedLocales = catfile($workdir, 'shipped-locales');
    my $diffFile = catfile($workdir, 'update-verify.diff');

    print "Running test on: $product, $osname, version $version, majorMode $majorMode, testOlderPartials: $testOlderPartials.....";

    # Create shipped-locales
    open(SHIPPED_LOCALES, ">$shippedLocales");
    print SHIPPED_LOCALES "af\n";
    print SHIPPED_LOCALES "en-US\n";
    print SHIPPED_LOCALES "gu-IN linux win32\n";
    print SHIPPED_LOCALES "ja linux win32\n";
    print SHIPPED_LOCALES "ja-JP-mac osx\n";
    print SHIPPED_LOCALES "uk\n";
    close(SHIPPED_LOCALES);

    my $localeInfo = {};
    if (not LoadLocaleManifest(localeHashRef => $localeInfo,
                               manifest => $shippedLocales)) {
        die "Could not load locale manifest";
    }

    # Create a verify config to bump
    my @oldOldRelease = (
        "release=\"$oldOldVersion\"",
        "product=\"$brand\"",
        "platform=\"$platform\"",
        "build_id=\"$oldOldBuildid\"",
        "locales=\"af de en-US gu-IN ja ru uk\"",
        "channel=\"betatest\"",
        "patch_types=\"partial complete\"",
        "from=\"$oldOldFromBuild\"",
        "aus_server=\"$ausServer\"",
        "ftp_server=\"$stagingServer/pub/mozilla.org\"",
        "to=\"$oldOldToBuild\""
    );

    open(CONFIG, ">$bumpedConfig");
    print CONFIG "# $oldOldVersion $osname\n";
    print CONFIG join(' ', @oldOldRelease);
    close(CONFIG);

    # Prepare the reference file...
    # BumpVerifyConfig removes everything after 'from' or 'patch_types' for the
    # for the previous release, depending on the options passed.
    # So must we in the reference file.
    my @oldOldReleaseQuickCheck = (
        "release=\"$oldOldVersion\"",
        "product=\"$brand\"",
        "platform=\"$platform\"",
        "build_id=\"$oldOldBuildid\"",
        "locales=\"af gu-IN ja uk\"",
        "channel=\"betatest\"",
    );
    my @oldOldReleaseFullCheck = (
        "release=\"$oldOldVersion\"",
        "product=\"$brand\"",
        "platform=\"$platform\"",
        "build_id=\"$oldOldBuildid\"",
        "locales=\"de en-US ru\"",
        "channel=\"betatest\"",
    );
    if ( $testOlderPartials ) {
        push(@oldOldReleaseQuickCheck, "patch_types=\"partial complete\"");
        push(@oldOldReleaseFullCheck, "patch_types=\"partial complete\"");
    }
    else {
        $oldOldReleaseQuickCheck[-1] .= "";
    }
    push(@oldOldReleaseFullCheck, "from=\"$oldOldFromBuild\"");

    # Now create an already bumped configuration to file
    my @oldRelease = (
        "release=\"$oldVersion\"",
        "product=\"$brand\"",
        "platform=\"$platform\"",
        "build_id=\"$oldBuildid\"",
        "locales=\"af en-US gu-IN ja uk\"",
        "channel=\"betatest\""
    );
    if ( ! $majorMode || $testOlderPartials ) {
        push(@oldRelease, "patch_types=\"partial complete\"");
    }
    push(@oldRelease,
        "from=\"$oldFromBuild\"",
        "aus_server=\"$ausServer\"",
        "ftp_server=\"$stagingServer/pub/mozilla.org\"",
        "to=\"$oldToBuild\""
    );

    open(CONFIG, ">$referenceConfig");
    print CONFIG "# $oldVersion $osname\n";
    print CONFIG join(' ', @oldRelease) . "\n";
    if ( ! $majorMode ) {
        print CONFIG "# $oldOldVersion $osname - full check\n";
        print CONFIG join(' ', @oldOldReleaseFullCheck) . "\n";
        print CONFIG "# $oldOldVersion $osname - quick check\n";
        print CONFIG join(' ', @oldOldReleaseQuickCheck) . "\n";
    }
    close(CONFIG);
    
    # We need to build up the global config object before calling
    # BumpVerifyConfig
    $config{'osname'} = $osname;
    $config{'product'} = $product;
    $config{'brand'} = $brand;
    $config{'binary-name'} = $binaryName;
    $config{'old-binary-name'} = $oldBinaryName;
    $config{'old-version'} = $oldVersion;
    $config{'old-app-version'} = $oldVersion;
    $config{'old-long-version'} = $oldLongVersion;
    $config{'version'} = $version;
    $config{'app-version'} = $version;
    $config{'long-version'} = $longVersion;
    $config{'build-number'} = $build;
    $config{'aus-server-url'} = $ausServer;
    $config{'staging-server'} = $stagingServer;
    $config{'verify-config'} = $bumpedConfig;
    $config{'old-candidates-dir'} = $oldCandidatesDir;
    $config{'shipped-locales'} = $shippedLocales;
    $config{'linux-extension'} = $linuxExtension;
    $config{'pretty-candidates-dir'} = $prettyCandidatesDir;
    $config{'major'} = $majorMode;
    $config{'test-older-partials'} = $testOlderPartials;

    BumpVerifyConfig();

    system("diff -au $referenceConfig $bumpedConfig > $diffFile");
    if ($? != 0) {
        print "FAIL: \n";
        print "  Bumped file and reference file differ. Please inspect " .
                "$diffFile for details.\n";
    }
    else {
        print "PASS\n";
        # Now, cleanup the tempdir
        rmtree($workdir);
    }
}
