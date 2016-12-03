#!/usr/bin/perl -w
# -*- cperl -*-
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Remote Credential Updater.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):  Reed Loden <reed@mozilla.com>
#                  Chris Cooper <ccooper@deadsquid.com>
#                  Nick Thomas <nthomas@mozilla.com>
#
use strict;
$|++;

$ENV{PATH} = "/bin:/usr/bin:/usr/local/bin";

use Data::Dumper;
use Date::Manip;
use diagnostics;
use warnings;
use Getopt::Long;
use LWP;
use Net::SSH::Expect;

use vars qw(
	    $verbose
	    $machine_list
	    $host_errors
	    $default_ssh_timeout
	    $start_timestamp
           );

$default_ssh_timeout = 5;
$start_timestamp = UnixDate("now","%q");

#########################################################################
sub loadExistingMachineList($) {
  my $machine_list_file = shift;

  print "Loading existing machine list: $machine_list_file..." if ($verbose);

  if (-e $machine_list_file and
      -r $machine_list_file) {
    open(MACHINELIST, $machine_list_file) or die "Unable to open machine list file: $machine_list_file";
    undef $machine_list;
    while (<MACHINELIST>) {
      chomp;
      next if (/^\s*$/);
      my ($machine_name,$platform,$timeout) = split(',',$_);
      $machine_list->{$machine_name}->{'platform'} = $platform;
      $machine_list->{$machine_name}->{'timeout'} = $timeout || $default_ssh_timeout;
    }
    close(MACHINELIST);

    print "Done.\n" if ($verbose);

    return;
  }

  warn "Unable to find machine list file: $machine_list_file";
  return undef;
}

#########################################################################
sub updateMachineListFromWeb($$$) {
  my $http_user = shift;
  my $http_password = shift;
  my $skip_known = shift;

  print "Updating machine list from web...\n" if ($verbose);

  my $browser = LWP::UserAgent->new;
  $browser->credentials(
			'build.inventory.mozilla.org:443',
			'Inventory - LDAP Login',
			 $http_user => $http_password
		       );

  my $csv_url = 'https://build.inventory.mozilla.org/build/csv';
  my $response = $browser->get($csv_url);

  die "Error at $csv_url\n ", $response->status_line, "\n Aborting"
    unless $response->is_success;

  my @csv = split("\n", $response->content);

  foreach my $line (@csv) {
    $line =~ s/^"//g;
    $line =~ s/"$//g;
    $line =~ s/,(.*)$//g;
    $line =~ s/^\s*//g;
    $line =~ s/\s*$//g;
    next if ($line =~ /^Host Name/);
    next if ($line =~ /^,/);
    next if ($line =~ /^\w*$/);
    if ($machine_list and 
	$machine_list->{$line} and
        $skip_known) {
      print "Already know about $line, skipping...\n";
      next;
    }

    # Ignore our ref VMs since they're not powered on by default anyway.
    if ($line =~ /-ref-vm/ or
	$line =~ /-ref-tools-vm/) {
      $machine_list->{$line}->{'platform'} = 'skipped_refvm';
      next;
    }

    if ($line =~ /xserve/i or
	$line =~ /mac[^h]/i or
	$line =~ /darwin/i or
	$line =~ /tiger/i or
	$line =~ /snow/i or
	$line =~ /osx/i) {
      $machine_list->{$line}->{'platform'} = 'mac';
      next;
    }

    if ($line =~ /win/i or
	$line =~ /w32/ or
	$line =~ /w64/ or
	$line =~ /xp/i or
	$line =~ /vista/i or
	$line =~ /r3-w7/i or
	$line =~ /2k3/i) {
      $machine_list->{$line}->{'platform'} = 'win32';
      next;
    }

    if ($line =~ /linux/i or
	$line =~ /centos/i or
	$line =~ /r3-fed/i or
	$line =~ /ubuntu/i) {
      $machine_list->{$line}->{'platform'} = 'linux';
      next;
    }

    $machine_list->{$line}->{'platform'} = 'unknown';
  }

}

#########################################################################
sub updateMachineListEntry($$$$) {
  my $old_machine_name = shift;
  my $new_machine_name = shift;
  my $new_platform = shift;
  my $new_timeout = shift;

  if (!$old_machine_name or
      !$machine_list->{$old_machine_name}) {
    return undef;
  }

  if (!$new_platform) {
    $new_platform = $machine_list->{$old_machine_name}->{'platform'};
    $new_timeout = $machine_list->{$old_machine_name}->{'timeout'};
  }

  if (!$new_timeout) {
    $new_timeout = $machine_list->{$old_machine_name}->{'timeout'} || $default_ssh_timeout;
  } 

  if (!$new_machine_name) {
    $new_machine_name = $old_machine_name;
  } else {
    delete $machine_list->{$old_machine_name};
  }

  $machine_list->{$new_machine_name}->{'platform'} = $new_platform;
  $machine_list->{$new_machine_name}->{'timeout'} =
    $new_timeout || $default_ssh_timeout;

  return 1;
}

#########################################################################
sub writeMachineListToDisk($) {
  my $new_machine_list_file = shift;

  $new_machine_list_file =~ s/\.\d{14,14}$//g;
  $new_machine_list_file .= '.' . $start_timestamp;

  print "Writing machine list to disk: $new_machine_list_file..." if ($verbose);

  open(MACHINELIST, ">$new_machine_list_file") or
    die "Unable to write out machine list: $new_machine_list_file";
  foreach my $machine_name (sort keys %{$machine_list}) {
    my $platform = $machine_list->{$machine_name}->{'platform'} || 'unknown';
    my $timeout = $machine_list->{$machine_name}->{'timeout'} || $default_ssh_timeout;
    print MACHINELIST "${machine_name},${platform},${timeout}\n";
  }
  close(MACHINELIST);

  print "Done.\n" if ($verbose);

  return $new_machine_list_file;
}


#########################################################################
sub updateRemotePasswordViaSSH($$$$$) {
  my $host = shift;
  my $username = shift;
  my $current_password = shift;
  my $new_password = shift;
  my $new_vnc_password = shift;

  my $platform = $machine_list->{$host}->{'platform'};
  my $timeout = $machine_list->{$host}->{'timeout'} || $default_ssh_timeout;

  # Our hostname may be missing a domain suffix.
  my @hostnames;
  push @hostnames, $host;
  if ($host !~ /mozilla\.org/i) {
    push @hostnames, "$host.build.mozilla.org", "$host.mozilla.org";
  }

  foreach my $hostname (@hostnames) {
    undef $@;
    eval {
      # Construct the SSH object
      my $ssh = Net::SSH::Expect->new (
				       host => $hostname,
				       user => $username,
				       password => $current_password,
				       raw_pty => 1,
				       ssh_option => '-o StrictHostKeyChecking=no',
				       timeout => $timeout
				      );
      # Log in to the SSH server using those credentials.
      if (!$ssh->run_ssh()) {
	die "unreachable";
      }

      # If a password was supplied, check if key-based
      # authentication worked; if not, try using the password

      # Due to the difficulties of dealing with remote timing, we append
      # remote output and clear it when we're in a known state, 
      # i.e. command prompt.
      my $remote_output = "";
      if ($current_password) {
	$remote_output = $ssh->peek($timeout);
	if ($remote_output =~ /[pP]assword:\s*$/) {
	  # send the password if password-based authentication
	  $remote_output .= $ssh->login();
	  # test the login output to make sure we had success
	  if ($remote_output =~ /Permission denied/) {
	    die "login_failed_$username";
	  }
	}
      }

      $remote_output .= $ssh->peek($timeout);

      # Should be logged in now. Test if received the remote prompt:
      if ($remote_output !~ m/(\$|#|>)\s*\z/) {
	die "no_prompt_$username";
      }

      # - all logged in - #

      $remote_output = "";

      # Change the user's password to new password
      if ($current_password and 
	  $new_password and
	  $new_password ne $current_password) {
	$ssh->send("passwd");
	if ($username ne 'root') {
	  if (! $ssh->waitfor('[Pp]assword:\s*\z', $timeout)) {
	    push @{$host_errors->{$host}}, "User $username: current password prompt not found after $timeout seconds";
	    return 0;
	  }
	  $ssh->send($current_password);
	}
	if (! $ssh->waitfor('[Pp]assword:\s*\z', $timeout)) {
	  push @{$host_errors->{$host}}, "User $username: new password prompt not found after $timeout seconds";
	  return 0;
	}
	$ssh->send($new_password);
	if (! $ssh->waitfor('[Pp]assword:\s*\z', $timeout)) {
	  push @{$host_errors->{$host}}, "User $username: confirm password prompt not found after $timeout seconds";
	  return 0;
	}
	$ssh->send($new_password);
	# We insert a short pause here, otherwise the two sends seem to get
	# concatenated, screwing up our password change.
	$ssh->peek(1);
	$ssh->send("echo \"success: \$?\"");

	# Check to see if the password change was successful
	if (! $ssh->waitfor("success: 0", $timeout, "-ex")) {
	  push @{$host_errors->{$host}}, "User $username: passwd changed failed.";
	  return 0;
	} else {
	  print "Password changed for $username on $host\n" if ($verbose);
	}
      }

      $remote_output = "";

      # Change the VNC password, but only if we're not root.
      if ($username ne 'root' and
	  $new_vnc_password and
	  $new_vnc_password ne "") {
	if ($platform eq "mac") {
	  # On OS X (Darwin), use kickstart but require password for sudo
	  if ($current_password) {
	    # get Darwin version (eg 8.8.1, 9.2.0, 10.2.0)
	    $ssh->send("uname -v");
	    $remote_output = $ssh->peek($timeout);
	    my $vnc_password;
	    # use the xor'd password for Tiger, otherwise plain text
	    if ($remote_output =~ /Darwin Kernel Version 8\./) {
	      $vnc_password = $new_vnc_password;
	    } else {
	      $vnc_password = $new_password;
	    }

	    $remote_output = "";
	    $ssh->send("sudo /System/Library/CoreServices/RemoteManagement/ARDAgent.app/Contents/Resources/kickstart -configure -activate -access -on -clientopts -setvnclegacy -vnclegacy yes -setvncpw -vncpw $vnc_password -restart -agent");
	    if (! $ssh->waitfor('Password:\s*\z', $timeout)) {
	      push @{$host_errors->{$host}}, "User $username: VNC prompt 'Password' not found after $timeout seconds";
	      return 0;
	    }
	    $ssh->send($new_password);
	    $remote_output .= $ssh->peek($timeout);
#	    print STDERR $remote_output;
	    if ($remote_output =~ m/Done\./) {
	      print "VNC password changed on $host.\n" if ($verbose);
	    } else {
	      push @{$host_errors->{$host}}, "User $username: Never saw VNC command return succesfully.";
	      return 0;
	    }

	    # write auto-logon password
	    my $kcpassword = kcpassword_xor($new_password);
	    $ssh->send("echo -e '$kcpassword' > ~/kcpassword");
	    $ssh->send('sudo cp ~/kcpassword /etc/kcpassword');
	    # sudo has cached our credentials, no password needed
	    $ssh->send('rm -f ~/kcpassword');

	    # blow away default keychain, so we don't get prompted that the
	    # passwords for the user account and keychain differ on 10.6
	    $ssh->send('rm -f ~/Library/Keychains/login.keychain');

	    # need to flush the output to make things happen
	    $remote_output = $ssh->peek(1);
	  }
	} elsif ($platform eq "linux") {
	  # On Linux, use vncpasswd
	  $ssh->send("vncpasswd");
	  if (! $ssh->waitfor('Password:\s*\z', $timeout)) {
	    push @{$host_errors->{$host}}, "User $username: VNC prompt 'Password' not found after $timeout seconds";
	    return 0;
	  }
	  $ssh->send($new_vnc_password);
	  if (! $ssh->waitfor('Verify:\s*\z', $timeout)) {
	    push @{$host_errors->{$host}}, "User $username: VNC prompt 'Verify:' not found";
	    return 0;
	  }
	  $ssh->send($new_vnc_password);
	  # Again, we insert a short pause here, otherwise the two sends 
	  # seem to get concatenated, screwing up our password change.
	  $ssh->peek(1);
	  $ssh->send("echo \"success: \$?\"");
	  # Check to see if the password change was successful
	  if (! $ssh->waitfor("success: 0", $timeout, "-ex")) {
	    push @{$host_errors->{$host}}, "User $username: vncpasswd change failed.";
	    return 0;
	  }
	  # vncpasswd said: ".$ssh->before()
	  print "VNC password changed for $username on $host.\n" if ($verbose);
	}
      }

      # If we had to amend the hostname to connect, make that change in the 
      # machine list to speed up the next iteration.
      updateMachineListEntry($host,$hostname,undef,undef);

      # Close the SSH connection
      $ssh->close();
      return 0;
    };

    next if ($@);
    last;
  }

  if ($@) {
    my $status = $platform;
    if ($@ =~ /^SSHProcessError/) {
      $status .= "(unreachable)";
    } else {
      my ($short_error,undef) = split(' ',$@,2);
      $status .= "(${short_error})";
    }
    updateMachineListEntry($host,undef,$status,undef);
    push @{$host_errors->{$host}}, $@ || "unreachable";
    return 0;
  }

  return 1;
}

#########################################################################
sub confirmSSHConnection($$$) {
  my $host = shift;
  my $username = shift;
  my $current_password = shift;

  my $platform = $machine_list->{$host}->{'platform'};
  my $timeout = $machine_list->{$host}->{'timeout'} || $default_ssh_timeout;

  # Our hostname may be missing a domain suffix.
  my @hostnames;
  push @hostnames, $host;
  if ($host !~ /mozilla\.org/i) {
    push @hostnames, "$host.build.mozilla.org", "$host.mozilla.org";
  }

  foreach my $hostname (@hostnames) {
    undef $@;
    eval {
      # Construct the SSH object
      my $ssh = Net::SSH::Expect->new (
				       host => $hostname,
				       user => $username,
				       password => $current_password,
				       raw_pty => 1,
				       ssh_option => '-o StrictHostKeyChecking=no',
				       timeout => $timeout
				      );
      # Log in to the SSH server using those credentials.
      if (!$ssh->run_ssh()) {
	die "unreachable";
      }

      # If a password was supplied, check if key-based
      # authentication worked; if not, try using the password

      # Due to the difficulties of dealing with remote timing, we append
      # remote output and clear it when we're in a known state, 
      # i.e. command prompt.
      my $remote_output = "";
      if ($current_password) {
	$remote_output = $ssh->peek($timeout);
	if ($remote_output =~ /[pP]assword:\s*$/) {
	  # send the password if password-based authentication
	  $remote_output .= $ssh->login();
	  # test the login output to make sure we had success
	  if ($remote_output =~ /Permission denied/) {
	    die "login_failed_$username";
	  }
	}
      }

      $remote_output .= $ssh->peek($timeout);

      # Should be logged in now. Test if received the remote prompt:
      if ($remote_output !~ m/(\$|#|>)\s*\z/) {
	die "no_prompt_$username";
      }

      # - all logged in - #

      $remote_output = "";
      $ssh->send("whoami");
      $remote_output = $ssh->peek($timeout);

      if ($remote_output !~ /$username/) {
        die "wrong_username_$username";
      }

      # If we had to amend the hostname to connect, make that change in the 
      # machine list to speed up the next iteration.
      updateMachineListEntry($host,$hostname,undef,undef);

      # Close the SSH connection
      $ssh->close();
      return 0;
    };

    next if ($@);
    last;
  }

  if ($@) {
    my $status = $platform;
    if ($@ =~ /^SSHProcessError/) {
      $status .= "(unreachable)";
    } else {
      my ($short_error,undef) = split(' ',$@,2);
      $status .= "(${short_error})";
    }
    updateMachineListEntry($host,undef,$status,undef);
    push @{$host_errors->{$host}}, $@ || "unreachable";
    return 0;
  }

  return 1;
}

#########################################################################
sub writeHostErrorsToDisk($) {
  my $username = shift;

  my $host_errors_log = "./";
  if ($username and $username ne "") {
    $host_errors_log .= $username . "_";
  }
  $host_errors_log .= "host_errors_" . $start_timestamp . ".log";
  open(HOSTERRORS, ">$host_errors_log") or die "Couldn't open $host_errors_log: $!";
  if (!$host_errors) {
    print HOSTERRORS "$0 encountered no errors.\n\n" if ($verbose);
    close(HOSTERRORS);
    return 1;
  }

  foreach my $host (sort keys %$host_errors) {
    if (scalar(@{$host_errors->{$host}}) > 0) {
      foreach my $error_msg (@{$host_errors->{$host}}) {
	chomp $error_msg;
	print HOSTERRORS "$host: $error_msg\n";
      }
    }
  }

  close(HOSTERRORS);
}

#########################################################################
sub diffMachineLists($$) {
  my $machine_list_file = shift;
  my $new_machine_list_file = shift;

  if (-r $machine_list_file and
      -r $new_machine_list_file) {
    system("diff -up $machine_list_file $new_machine_list_file");
  }
}

#########################################################################
# credit: http://www.brock-family.org/gavin/perl/kcpassword.html
# and then convert to something we can push through a shell
sub kcpassword_xor($) {
  my $pass = shift;

  ### The magic 11 bytes - these are just repeated 
  # 0x7D 0x89 0x52 0x23 0xD2 0xBC 0xDD 0xEA 0xA3 0xB9 0x1F
  my @key = qw( 125 137 82 35 210 188 221 234 163 185 31 );

  my $key     = pack "C*", @key;
  my $key_len = length $key;

  for (my $n=0; $n<length($pass); $n+=$key_len) {
    substr($pass,$n,$key_len) ^= $key;
  }
  # convert to string suitable for echo -e in a terminal
  $pass =~ s/(.)/sprintf('\x%x',ord($1))/eg;
  return $pass;
}

#########################################################################
sub usage() {
  print<<EOS;
Usage: $0 --username=X --current_password=X
          [--new_password=X] [--verbose] [--machine_list_file=X] \
          [--update_from_web --http_user=X --http_password=X \
                             --skip_known  --update_only] \
          [--platform=mac|linux] \
          [--dry_run]

 --username=X          : the username to use when connceting
 --current_password=X  : the current password for \$username
 --new_password=X      : the new password for \$username. Not required for a
                         --dry_run
 --verbose             : narrate the action
 --machine_list_file=X : path to an existing machine list file
 --update_from_web     : update the list of machines from the build inventory 
                         on the web
 --http_user=X         : user to use when connecting to the build inventory
 --http_password=X     : password to use when connecting to the build inventory
 --update_only         : update the machine list from the web and then exit.
                         Useful as a first pass if you want to tweak the 
                         machine list prior to running.
 --skip-known          : when updating from the web, don't clobber the
                         descriptions for machines we already know about.
                         This is useful for just looking up new machines.
 --dry_run             : simply try to connect to the remote machines without
                         changing any passwords. This is a good way to prune
                         and/or verify your machine before doing anything 
                         rash.
 --platform=X          : limit the script to updating one platform only.
                         Valid options are "linux" or "mac"

EOS
}

#########################################################################

my $machine_list_file = "";
my $http_user;
my $http_password;
my $update_from_web = 0;
my $update_only = 0;
my $skip_known = 0;
my $dry_run = 0;
my $username = "";
my $current_password = "";
my $new_password = "";
my $help;
my $platform;

$verbose = 0;

GetOptions(
	   "http_user=s" => \$http_user,
	   "http_password=s" => \$http_password,
	   "update_from_web" => \$update_from_web,
           "update_only" => \$update_only,
	   "skip_known" => \$skip_known,
	   "verbose" => \$verbose,
           "dry_run" => \$dry_run,
           "username=s" => \$username,
	   "current_password=s" => \$current_password,
	   "new_password=s" => \$new_password,
	   "machine_list_file=s" => \$machine_list_file,
	   "help" => \$help,
	   "platform=s" => \$platform,
	  );

if ($help) {
  usage();
  exit;
}

if ($platform and $platform ne "mac" and $platform ne "linux") {
  print STDERR "The only supported platforms are 'mac' and 'linux.'\n\n";
  usage();
  exit 1;
}

if (!$username and !$update_only) {
  print STDERR "No username supplied.\n\n";
  usage();
  exit 2;
}

if (!$current_password and !$update_only) {
  print STDERR "Current password not supplied.\n\n";
  usage();
  exit 3;
}

if (!$new_password and !$dry_run and !$update_only) {
  print STDERR "New password not supplied.\n\n";
  usage();
  exit 3;
}

if (!$machine_list_file) {
  $machine_list_file = "./build_machine_list_for_$username.txt";
}

loadExistingMachineList($machine_list_file);
if (!$machine_list) {
  if (!$http_user or
      !$http_password) {
    die "Unable to load existing machine list, and we have no auth credentials to grab it from the web.\n";
  }
  $update_from_web = 1;
}

if ($update_from_web and
    $http_user and
    $http_password) {
  updateMachineListFromWeb($http_user,
			   $http_password,
			   $skip_known);
}

if ($update_only) {
  my $new_machine_list_file = writeMachineListToDisk($machine_list_file);
  my $rv = diffMachineLists($machine_list_file,
			    $new_machine_list_file);
  exit;
}

if (!$machine_list) {
  die "Failed to find any machines. Exiting.\n";
}

if ($platform) {
  if (!$dry_run) {
    print "Changing passwords on all $platform machines...\n";
  }
} else {
  if (!$dry_run) {
    print "Changing passwords on all machines...\n";
  }
}

if ($dry_run) {
  foreach my $machine_name (sort keys %{$machine_list}) {
    next if ($machine_name =~ /^#/);
    next if ($machine_list->{$machine_name}->{'platform'} ne "mac" and
	     $machine_list->{$machine_name}->{'platform'} ne "linux");
    if ($platform) {
      next if ($machine_list->{$machine_name}->{'platform'} ne $platform);
    }
    print "Connecting as $username to $machine_name...\n" if ($verbose);
    my $rv = confirmSSHConnection(
				  $machine_name,
				  $username,
				  $current_password,
				 );
  }

} else {
  my $new_vnc_password = $new_password;
  $new_vnc_password =~ s/^(.{8}).*/$1/;  
  # convert the password to an array
  my @vnc_password_array = unpack "C*", $new_vnc_password;
  # XOR key
  my @vnc_xor_key = unpack "C*", pack "H*", "1734516E8BA8C5E2FF1C39567390ADCA";

  my $xor_vnc_password = "";
  foreach my $byte_value (@vnc_xor_key) {
    $xor_vnc_password .= sprintf("%02X",$byte_value ^ (shift @vnc_password_array || 0));
  }

  print STDERR "New VNC password (xor): $xor_vnc_password\n" if ($verbose);

  foreach my $machine_name (sort keys %{$machine_list}) {
    next if ($machine_name =~ /^#/);
    next if ($machine_list->{$machine_name}->{'platform'} ne "mac" and
	     $machine_list->{$machine_name}->{'platform'} ne "linux");
    if ($platform) {
      next if ($machine_list->{$machine_name}->{'platform'} ne $platform);
    }

    print "Updating " . $username . " on $machine_name...\n" if ($verbose);

    my $vnc_password = $new_password;
    if ($machine_list->{$machine_name}->{'platform'} eq 'mac') {
      $vnc_password = $xor_vnc_password;
    }
    my $rv = updateRemotePasswordViaSSH(
					$machine_name,
					$username,
					$current_password,
					$new_password,
					$vnc_password
				       );
    next if ($rv);
  }
}

my $rv = writeHostErrorsToDisk($username);

my $new_machine_list_file = writeMachineListToDisk($machine_list_file);
$rv = diffMachineLists($machine_list_file,
		       $new_machine_list_file);

exit;
