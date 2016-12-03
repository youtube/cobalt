<?php

include("clobberer_creds.php");

// disallow unless the correct token is given
if (getenv('CLEANUP_PASSWORD') != $CLEANUP_PASSWORD) {
  echo "wrong CLEANUP_PASSWORD\n";
  exit(1);
}

$dbh = new PDO($CLOBBERER_DSN, $CLOBBERER_USERNAME,
        $CLOBBERER_PASSWORD, $CLOBBERER_PDO_OPTIONS);
if (!$dbh) {
  print("Error: couldn't connect\n");
  print($error);
  exit(1);
}

header("Content-type: text/plain");
if (substr($CLOBBERER_DSN, 0, 5) == 'mysql') {
	$q = "delete from builds where last_build_time < unix_timestamp(adddate(now(), interval -21 day)) and buildername not like 'rel%'";
	$dbh->exec($q) === false and die(print_r($dbh->errorInfo(), TRUE));

	$q = "delete from clobber_times where lastclobber < unix_timestamp(adddate(now(), interval -21 day))";
	$dbh->exec($q) === false and die(print_r($dbh->errorInfo(), TRUE));

	// optimize table?  Only reclaims table space, so probably not helpful..
} else {
	// sqlite
	$q = "delete from builds where last_build_time < strftime('%s', 'now', '-21 days') and buildername not like 'rel%'";
	$dbh->exec($q) === false and die(print_r($dbh->errorInfo(), TRUE));

	$q = "delete from clobber_times where lastclobber < strftime('%s', 'now', '-21 days')";
	$dbh->exec($q) === false and die(print_r($dbh->errorInfo(), TRUE));

	$q = "vacuum";
	$dbh->exec($q) === false and die(print_r($dbh->errorInfo(), TRUE));
}
