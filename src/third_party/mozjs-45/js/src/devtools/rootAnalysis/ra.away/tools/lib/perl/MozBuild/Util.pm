package MozBuild::Util;

use strict;

use File::Path;
use IO::Handle;
use IO::Select;
use IPC::Open3;
use POSIX qw(:sys_wait_h);
use File::Basename qw(fileparse);
use File::Copy qw(move);
use File::Temp qw(tempfile);
use File::Spec::Functions;
use Cwd;

use base qw(Exporter);

our @EXPORT_OK = qw(RunShellCommand MkdirWithPath HashFile DownloadFile Email
                    UnpackBuild GetBuildIDFromFTP);

my $DEFAULT_EXEC_TIMEOUT = 600;
my $EXEC_IO_READINCR = 1000;
my $EXEC_REAP_TIMEOUT = 10;

# RunShellCommand is a safe, performant way that handles all the gotchas of
# spawning a simple shell command. It's meant to replace backticks and open()s,
# while providing flexibility to handle stdout and stderr differently, provide
# timeouts to stop runaway programs, and provide easy-to-obtain information
# about return values, signals, output, and the like.
#
# Arguments:
#    command - string: the command to run
#    args - optional array ref: list of arguments to an array
#    logfile - optional string: logfile to copy output to
#    timeout - optional int: timeout value, in seconds, to wait for a command;
#     defaults to ten minutes; set to 0 for no timeout
#    redirectStderr - bool, default true: redirect child's stderr onto stdout
#     stream?
#    appendLogfile - bool, default true: append to the logfile (as opposed to
#     overwriting it?)
#    printOutputImmedaitely - bool, default false: print the output here to
#     whatever is currently defined as *STDOUT?
#    background - bool, default false: spawn a command and return all the info
#     the caller needs to handle the program; assumes the caller takes
#     complete responsibility for waitpid(), handling of the stdout/stderr
#     IO::Handles, etc.
#

sub RunShellCommand {
    my %args = @_;

    my $shellCommand = $args{'command'};
    die('ASSERT: RunShellCommand(): Empty command.')
     if (not(defined($shellCommand)) || $shellCommand =~ /^\s+$/);
    my $commandArgs = $args{'args'};
    die('ASSERT: RunShellCommand(): commandArgs not an array ref.')
     if (defined($commandArgs) && ref($commandArgs) ne 'ARRAY');

    my $logfile = $args{'logfile'};

    # Optional
    my $timeout = exists($args{'timeout'}) ?
     int($args{'timeout'}) : $DEFAULT_EXEC_TIMEOUT;
    my $redirectStderr = exists($args{'redirectStderr'}) ?
     $args{'redirectStderr'} : 1;
    my $appendLogfile = exists($args{'appendLog'}) ? $args{'appendLog'} : 1;
    my $printOutputImmediately = exists($args{'output'}) ? $args{'output'} : 0;
    my $background = exists($args{'bg'}) ? $args{'bg'} : 0;
    my $changeToDir = exists($args{'dir'}) ?  $args{'dir'} : undef;

    # This is a compatibility check for the old calling convention;
    if ($shellCommand =~ /\s/) {
        $shellCommand =~ s/^\s+//;
        $shellCommand =~ s/\s+$//;

        die("ASSERT: old RunShellCommand() calling convention detected\n")
         if ($shellCommand =~ /\s+/);
    }

    # Glob the command together to check for 2>&1 constructs...
    my $entireCommand = $shellCommand . 
     (defined($commandArgs) ? join (' ', @{$commandArgs}) : '');
   
    # If we see 2>&1 in the command, set redirectStderr (overriding the option
    # itself, and remove the construct from the command and arguments.
    if ($entireCommand =~ /2\>\&1/) {
        $redirectStderr = 1;
        $shellCommand =~ s/2\>\&1//g;
        if (defined($commandArgs)) {
            for (my $i = 0; $i < scalar(@{$commandArgs}); $i++) {
                $commandArgs->[$i] =~ s/2\>\&1//g;
            }
        }
    }

    local $_;
 
    chomp($shellCommand);

    my $cwd = getcwd();
    my $exitValue = undef;
    my $signalNum = undef;
    my $sigName = undef;
    my $dumpedCore = undef;
    my $childEndedTime = undef;
    my $timedOut = 0;
    my $output = '';
    my $childPid = 0;
    my $childStartedTime = 0;
    my $childReaped = 0;
    my $prevStdoutBufferingSetting = 0;
    local *LOGFILE;

    my $original_path = $ENV{'PATH'};
    if  ($args{'prependToPath'} or $args{'appendToPath'}) {
        $ENV{'PATH'} = AugmentPath(%args);
    }
                    
    if ($printOutputImmediately) {
        # We Only end up doing this if it's requested that we're going to print
        # output immediately. Additionally, we can't call autoflush() on STDOUT
        # here, because doing so automatically sets it to on (gee, thanks); 
        # $| is the only way to get the value.
        my $prevFd = select(STDOUT);
        $prevStdoutBufferingSetting = $|;
        select($prevFd);
        STDOUT->autoflush(1);
    }

    if (defined($changeToDir)) {
        chdir($changeToDir) or die("RunShellCommand(): failed to chdir() to "
         . "$changeToDir\n");
    }

    eval {
        local $SIG{'ALRM'} = sub { die("alarm\n") };
        local $SIG{'PIPE'} = sub { die("pipe\n") };
  
        my @execCommand = ($shellCommand);
        push(@execCommand, @{$commandArgs}) if (defined($commandArgs) && 
         scalar(@{$commandArgs} > 0));
    
        my $childIn = new IO::Handle();
        my $childOut = new IO::Handle();
        my $childErr = new IO::Handle();

        alarm($timeout);
        $childStartedTime = time();

        $childPid = open3($childIn, $childOut, $childErr, @execCommand);
        $childIn->close();

        if ($args{'background'}) {
            alarm(0);

            # Restore external state
            chdir($cwd) if (defined($changeToDir));
            if ($printOutputImmediately) {
                my $prevFd = select(STDOUT);
                $| = $prevStdoutBufferingSetting;
                select($prevFd);
            }

            return { startTime => $childStartedTime,
                     endTime => undef,
                     timedOut => $timedOut,
                     exitValue => $exitValue,
                     sigNum => $signalNum,
                     output => undef,
                     dumpedCore => $dumpedCore,
                     pid => $childPid,
                     stdout => $childOut,
                     stderr => $childErr };
        }

        if (defined($logfile)) {
            my $openArg = $appendLogfile ? '>>' : '>';
            open(LOGFILE, $openArg . $logfile) or 
             die('Could not ' . $appendLogfile ? 'append' : 'open' . 
              " logfile $logfile: $!");
            LOGFILE->autoflush(1);
        }

        my $childSelect = new IO::Select();
        $childSelect->add($childErr);
        $childSelect->add($childOut);
    
        # Should be safe to call can_read() in blocking mode, since,
        # IF NOTHING ELSE, the alarm() we set will catch a program that
        # fails to finish executing within the timeout period.

        while (my @ready = $childSelect->can_read()) {
            foreach my $fh (@ready) {
                my $line = undef;
                my $rv = $fh->sysread($line, $EXEC_IO_READINCR);

                # Check for read()ing nothing, and getting errors...
                if (not defined($rv)) {
                    warn "sysread() failed with: $!\n";
                    next;
                }

                # If we didn't get anything from the read() and the child is
                # dead, we've probably exhausted the buffer, and can stop
                # trying...
                if (0 == $rv) {
                   $childSelect->remove($fh) if ($childReaped);
                   next;
                }

                # This check is down here instead of up above because if we're
                # throwing away stderr, we want to empty out the buffer, so
                # the pipe isn't constantly readable. So, sysread() stderr,
                # alas, only to throw it away.
                next if (not($redirectStderr) && ($fh == $childErr));

                $output .= $line;
                print STDOUT $line if ($printOutputImmediately);
                print LOGFILE $line if (defined($logfile));
            }

            if (!$childReaped && (waitpid($childPid, WNOHANG) > 0)) {
                alarm(0);
                $childEndedTime = time();
                $exitValue = WEXITSTATUS($?);
                $signalNum = WIFSIGNALED($?) && WTERMSIG($?);
                $dumpedCore = WIFSIGNALED($?) && ($? & 128);
                $childReaped = 1;
            }
        }

        die('ASSERT: RunShellCommand(): stdout handle not empty')
         if ($childOut->sysread(undef, $EXEC_IO_READINCR) != 0);
        die('ASSERT: RunShellCommand(): stderr handle not empty')
         if ($childErr->sysread(undef, $EXEC_IO_READINCR) != 0);
    };

    if (defined($logfile)) {
        close(LOGFILE) or die("Could not close logfile $logfile: $!");
    }

    if ($@) {
        if ($@ eq "alarm\n") {
            $timedOut = 1;
            if ($childReaped) {
                die('ASSERT: RunShellCommand(): timed out, but child already '.
                 'reaped?'); 
            }

            if (kill('KILL', $childPid) != 1) {
                warn("SIGKILL to timed-out child $childPid failed: $!\n");
            }

            # Processes get 10 seconds to obey.
            eval {
                local $SIG{'ALRM'} = sub { die("alarm\n") };
                alarm($EXEC_REAP_TIMEOUT);
                my $waitRv = waitpid($childPid, 0);
                alarm(0);
                # Don't fill in these values if they're bogus.
                if ($waitRv > 0) {
                    $exitValue = WEXITSTATUS($?);
                    $signalNum = WIFSIGNALED($?) && WTERMSIG($?);
                    $dumpedCore = WIFSIGNALED($?) && ($? & 128);
                }
            };
        } else {
            warn "Error running $shellCommand: $@\n";
            $output = $@;
        }
    }

    # Restore path (if necessary)
    if ($ENV{'PATH'} ne $original_path) {
        $ENV{'PATH'} = $original_path;
    }

    # Restore external state
    chdir($cwd) if (defined($changeToDir));
    if ($printOutputImmediately) {
        my $prevFd = select(STDOUT);
        $| = $prevStdoutBufferingSetting;
        select($prevFd);
    }

    return { startTime => $childStartedTime,
             endTime => $childEndedTime,
             timedOut => $timedOut,
             exitValue => $exitValue,
             sigNum => $signalNum,
             output => $output,
             dumpedCore => $dumpedCore
           };
}

## Allows the path to be (temporarily) augmented on either end. It's expected
## that the caller will keep track of the original path if it needs to be
## reverted.
##
## NOTE: we don't actually set the path here, just return the augmented path
## string for use by the caller.

sub AugmentPath {
    my %args = @_;

    my $prependToPath = $args{'prependToPath'};
    my $appendToPath  = $args{'appendToPath'};
    
    my $current_path = $ENV{'PATH'};
    if ($prependToPath and $prependToPath ne '') {
        if ($current_path and $current_path ne '') {
            $current_path = $prependToPath . ":" . $current_path;
        } else {
            $current_path = $prependToPath;
        }
    }
    if ($appendToPath and $appendToPath ne '') {
        if ($current_path and $current_path ne '') {
            $current_path .= ":" . $appendToPath;
        } else {
            $current_path = $appendToPath;
        }
    }
    return $current_path;
}

## This is a wrapper function to get easy true/false return values from a
## mkpath()-like function. mkpath() *actually* returns the list of directories
## it created in the pursuit of your request, and keeps its actual success 
## status in $@. 

sub MkdirWithPath {
    my %args = @_;

    my $dirToCreate = $args{'dir'};
    my $printProgress = $args{'printProgress'};
    my $dirMask = undef;

    # Renamed this argument, since "mask" makes more sense; it takes
    # precedence over the older argument name.
    if (exists($args{'mask'})) {
        $dirMask = $args{'mask'};
    } elsif (exists($args{'dirMask'})) {
        $dirMask = $args{'dirMask'};
    }

    die("ASSERT: MkdirWithPath() needs an arg") if not defined($dirToCreate);

    ## Defaults based on what mkpath does...
    $printProgress = defined($printProgress) ? $printProgress : 0;
    $dirMask = defined($dirMask) ? $dirMask : 0777;

    eval { mkpath($dirToCreate, $printProgress, $dirMask) };
    return ($@ eq '');
}

sub HashFile {
   my %args = @_;
   die "ASSERT: HashFile(): null file\n" if (not defined($args{'file'}));

   my $fileToHash = $args{'file'};
   my $hashFunction = lc($args{'type'}) || 'sha512';
   my $dumpOutput = $args{'output'} || 0;
   my $ignoreErrors = $args{'ignoreErrors'} || 0;

   die 'ASSERT: HashFile(): invalid hashFunction; use md5/sha1/sha256/sha384/sha512: ' .
    "$hashFunction\n" if ($hashFunction !~ '^(md5|sha1|sha256|sha384|sha512)$');

   if (not(-f $fileToHash) || not(-r $fileToHash)) {
      if ($ignoreErrors) {
         return '';
      } else {
         die "ASSERT: HashFile(): unusable/unreadable file to hash\n"; 
      }
   }

   # We use openssl because that's pretty much guaranteed to be on all the
   # platforms we want; md5sum and sha1sum aren't.
   my $rv = RunShellCommand(command => 'openssl',
                            args => ['dgst', "-$hashFunction",
                                            $fileToHash, ],
                            output => $dumpOutput);
   
   if ($rv->{'timedOut'} || $rv->{'exitValue'} != 0) {
      if ($ignoreErrors) {
         return '';      
      } else {
         die("MozUtil::HashFile(): hash call failed: $rv->{'exitValue'}\n");
      }
   }

   my $hashValue = $rv->{'output'};
   chomp($hashValue);

   # Expects input like MD5(mozconfig)= d7433cc4204b4f3c65d836fe483fa575
   # Removes everything up to and including the "= "
   $hashValue =~ s/^.+\s+(\w+)$/$1/;
   return $hashValue;
}

sub Email {
    my %args = @_;

    my $from = $args{'from'};
    my $to = $args{'to'};
    my $ccList = $args{'cc'} ? $args{'cc'} : undef;
    my $subject = $args{'subject'};
    my $message = $args{'message'};
    my $sendmail = $args{'sendmail'};
    my $blat = $args{'blat'};

    if (not defined($from)) {
        die("ASSERT: MozBuild::Utils::Email(): from is required");
    } elsif (not defined($to)) {
        die("ASSERT: MozBuild::Utils::Email(): to is required");
    } elsif (not defined($subject)) {
        die("ASSERT: MozBuild::Utils::Email(): subject is required");
    } elsif (not defined($message)) {
        die("ASSERT: MozBuild::Utils::Email(): subject is required");
    }    

    if (defined($ccList) and ref($ccList) ne 'ARRAY') {
        die("ASSERT: MozBuild::Utils::Email(): ccList is not an array ref\n");
    }

    if (-f $sendmail) {
        open(SENDMAIL, "|$sendmail -oi -t")
          or die("MozBuild::Utils::Email(): Can't fork for sendmail: $!\n");
        print SENDMAIL "From: $from\n";
        print SENDMAIL "To: $to\n";
        foreach my $cc (@{$ccList}) {
            print SENDMAIL "CC: $cc\n";
        }
        print SENDMAIL "Subject: $subject\n\n";
        print SENDMAIL "\n$message";

        close(SENDMAIL);
    } elsif(-f $blat) {
        my ($mh, $mailfile) = tempfile(DIR => '.');

        my $toList = $to;
        foreach my $cc (@{$ccList}) {
            $toList .= ',';
            $toList .= $cc;
        }
        print $mh "\n$message";
        close($mh) or die("MozBuild::Utils::Email(): could not close tempmail file $mailfile: $!");

        my $rv = RunShellCommand(command => $blat,
                                 args => [$mailfile, '-to', $toList,
                                          '-subject', '"' . $subject . '"']);
        if ($rv->{'timedOut'} || $rv->{'exitValue'} != 0) {
          die("MozBuild::Utils::Email(): FAILED: $rv->{'exitValue'}," .
              " output: $rv->{'output'}\n");
        }

        print "$rv->{'output'}\n";

    } else {
        die("MozBuild::Utils::Email(): ASSERT: cannot find $sendmail or $blat");
    }
}

sub DownloadFile {
    my %args = @_;

    my $sourceUrl = $args{'url'};

    die("ASSERT: DownloadFile() Invalid Source URL: $sourceUrl\n") 
     if (not(defined($sourceUrl)) || $sourceUrl !~ m|^http://|);

    my @wgetArgs = ();
    
    if (defined($args{'dest'})) {
        push(@wgetArgs, ('-O', $args{'dest'}));
    }
   
    if (defined($args{'user'})) {
        push(@wgetArgs, ('--http-user', $args{'user'}));
    }

    if (defined($args{'password'})) {
        push(@wgetArgs, ('--http-password', $args{'password'}));
    }

    push(@wgetArgs, ('--progress=dot:mega', $sourceUrl));

    my $rv = RunShellCommand(command => 'wget',
                             args => \@wgetArgs);

    if ($rv->{'timedOut'} || $rv->{'exitValue'} != 0) {
        die("DownloadFile(): FAILED: $rv->{'exitValue'}," . 
         " output: $rv->{'output'}\n");
    }
}

##
# Unpacks Mozilla installer builds.
#
# Arguments:
#     file    - file to unpack
#     unpackDir - dir to unpack into
##

sub UnpackBuild {
    my %args = @_;

    my $hdiutil = defined($ENV{'HDIUTIL'}) ? $ENV{'HDIUTIL'} : 'hdiutil';
    my $rsync = defined($ENV{'RSYNC'}) ? $ENV{'RSYNC'} : 'rsync';
    my $tar = defined($ENV{'TAR'}) ? $ENV{'TAR'} : 'tar';
    my $sevenzip = defined($ENV{'SEVENZIP'}) ? $ENV{'SEVENZIP'} : '7z';

    my $file = $args{'file'};
    my $unpackDir = $args{'unpackDir'};

    if (! defined($file) ) {
        die("ASSERT: UnpackBuild: file is a required argument: $!");
    }
    if (! defined($unpackDir) ) {
        die("ASSERT: UnpackBuild: unpackDir is a required argument: $!");
    }
    if (! -f $file) {
        die("ASSERT: UnpackBuild: $file must exist and be a file: $!");
    }
    if (! -d $unpackDir) {
        mkdir($unpackDir) || die("ASSERT: UnpackBuld: could not create $unpackDir: $!");
    }

    my ($filename, $directories, $suffix) = fileparse($file, qr/[^.]*/);

    if (! defined($suffix) ) {
        die("ASSERT: UnpackBuild: no extension found for $filename: $!");
    }

    if ($suffix eq 'dmg') {
        my $mntDir = './mnt';
        if (! -d $mntDir) {
            mkdir($mntDir) || die("ASSERT: UnpackBuild: cannot create mntdir: $!");
        }
        # Note that this uses system() not RunShellCommand() because we need
        # to echo "y" to hdiutil, to get past the EULA code.
        system("echo \"y\" | PAGER=\"/bin/cat\" $hdiutil attach -quiet -puppetstrings -noautoopen -mountpoint ./mnt \"$file\"") || die ("UnpackBuild: Cannot unpack $file: $!");
        my $rv = RunShellCommand(command => $rsync,
                                 args => ['-av', $mntDir, $unpackDir]);
        if ($rv->{'timedOut'} || $rv->{'exitValue'} != 0) {
            die("UnpackBuild(): FAILED: $rv->{'exitValue'}," . 
                " output: $rv->{'output'}\n");
        }
        $rv = RunShellCommand(command => $hdiutil,
                              args => ['detach', $mntDir]);
        if ($rv->{'timedOut'} || $rv->{'exitValue'} != 0) {
            die("UnpackBuild(): FAILED: $rv->{'exitValue'}," . 
                " output: $rv->{'output'}\n");
        }
    }
    if ($suffix eq 'gz') {
        my $rv = RunShellCommand(command => $tar,
                                 args => ['-C', $unpackDir, '-zxf', $file]);
        if ($rv->{'timedOut'} || $rv->{'exitValue'} != 0) {
            die("UnpackBuild(): FAILED: $rv->{'exitValue'}," . 
                " output: $rv->{'output'}\n");
        }
    }
    if ($suffix eq 'exe') {
        my $oldpwd = getcwd();
        chdir($unpackDir);
        my $rv = RunShellCommand(command => $sevenzip,
                                 args => ['x', $file]);
        if ($rv->{'timedOut'} || $rv->{'exitValue'} != 0) {
            die("UnpackBuild(): FAILED: $rv->{'exitValue'}," . 
                " output: $rv->{'output'}\n");
        }
        chdir($oldpwd);
    }
}

sub GetBuildIDFromFTP {
    my %args = @_;

    my $os = $args{'os'};
    if (! defined($os)) {
        die("ASSERT: MozBuild::GetBuildIDFromFTP(): os is required argument");
    }
    my $releaseDir = $args{'releaseDir'};
    if (! defined($releaseDir)) {
        die("ASSERT: MozBuild::GetBuildIDFromFTP(): releaseDir is required argument");
    }
    my $stagingServer = $args{'stagingServer'};
    if (! defined($stagingServer)) {
        die("ASSERT: MozBuild::GetBuildIDFromFTP(): stagingServer is a required argument");
    }

    my ($bh, $buildIDTempFile) = tempfile(UNLINK => 1);
    $bh->close();
    my $info_url = 'http://' . $stagingServer . '/' . $releaseDir . '/' . $os . '_info.txt';
    my $rv = RunShellCommand(
      command => 'wget',
      args => ['-O', $buildIDTempFile,
               $info_url]
    );

    if ($rv->{'timedOut'} || $rv->{'dumpedCore'} || ($rv->{'exitValue'} != 0)) {
        die("wget of $info_url failed.");
    }

    my $buildID;
    open(FILE, "< $buildIDTempFile") || 
     die("Could not open buildID temp file $buildIDTempFile: $!");
    while (<FILE>) {
      my ($var, $value) = split(/\s*=\s*/, $_, 2);
      if ($var eq 'buildID') {
          $buildID = $value;
      }
    }
    close(FILE) || 
     die("Could not close buildID temp file $buildIDTempFile: $!");
    if (! defined($buildID)) {
        die("Could not read buildID from temp file $buildIDTempFile: $!");
    }
    if (! $buildID =~ /^\d+$/) {
        die("ASSERT: MozBuild::GetBuildIDFromFTP: $buildID is non-numerical");
    }
    chomp($buildID);

    return $buildID;
}

1;
