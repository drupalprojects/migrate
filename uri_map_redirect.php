<?php

/**
 * @file
 * Sample file for handling redirection from old to new URIs. Use an Apache
 * rewrite rule (or equivalent) to map legacy requests to this file. Customize
 * this file to your needs.
 *
 * TODO: Actually test this code.
 */

  // For security, this script is disabled by default.
  die('Comment out this line when you are ready to use this script');

  define('DRUPAL_ROOT', getcwd());
  require_once DRUPAL_ROOT . '/includes/bootstrap.inc';

  // Only bootstrap to DB so we are as fast as possible. Much of the Drupal API
  // is not available to us.
  drupal_bootstrap(DRUPAL_BOOTSTRAP_DATABASE);

  // You must populate this querystring param from a rewrite rule or $_SERVER
  // On Apache, we could likely use _SERVER['REDIRECT_URL']. nginx?
  $source_uri = $_GET['migrate_source_uri'];

  // This is a tall table mapping legacy URLs to source_id and migration_name.
  // If you can already know the migration name and source_id based on the URI,
  // then the first lookup is not needed.
  $uri_table = 'migrate_source_uri_map';

  if ($uri_map = db_query("SELECT migration_name, source_id FROM $uri_table WHERE source_uri = :source_uri", array(':source_uri' => $source_uri))->fetchObject()) {
    // Hurray, we do recognize this URI.
    // Consult migrate_map_x table to determine corresponding Drupal nid/tid/cid/etc.
    $map_table = 'migrate_map_' . $uri_map->migration_name;
    $sql = "SELECT destid1 FROM $map_table WHERE sourceid1 = :source_id";
    if ($migrate_map = db_query($sql, array(':source_id' => $uri_map->source_id))->fetchObject()) {
      // Hurray. We already migrated this content. Go there.
      header('Location: ' . migrate_build_url($migrate_map->destid1, $uri_map->migration_name), TRUE, 301);
    }
    else {
      // We recognize URI but don't have the content in Drupal. Very unlikely.
    }
  }
  else {
    // Can't find the source URI. TODO: Make nice 404 page.
    header('Status=Not Found', TRUE, 404);
    print 'Sorry folks. Park is closed.';
  }

  // Based on custom patterns, build the destination_uri for given source_uri
  function migrate_build_url($destid1, $migration_name) {
    // TODO: Add an entry for each migration that we need to redirect.
    variable_get('migrate_patterns', array(
      'blogentries' => 'node/:source_id',
      'slideshows' => 'node/:source_id',
      'tagterm' => 'taxonomy/term/:source_id',
    ));
    $pattern = $patterns[$migration_name];

    // Swap in the destination ID.
    $destination_uri = str_replace(':source_id1', $destid1, $pattern);

    // If willing to suffer major performance hit, build friendly URL.
    // drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
    // $destination_uri = drupal_get_path_alias($destination_uri);

    return  'http://' . $_SERVER['HTTP_HOST'] . '/' . $destination_uri;
  }

?>
