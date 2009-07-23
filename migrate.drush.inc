<?php
/**
 * Implementation of hook_drush_help().
 */
function migrate_drush_help($section) {
  switch ($section) {
    case 'drush:migrate all':
      return dt("Run the full migration process");
    case 'drush:migrate clear':
      return dt("Clear a given migration content set");
    case 'drush:migrate import':
      return dt("Import a given migration content set");
  }
}

/**
 * Implementation of hook_drush_command().
 */
function migrate_drush_command() {
  $migration_options = array(
    '--itemlimit' => 'The maximum number of items to migrate. If unspecified, all are migrated',
    '--feedback' => 'Frequency of progress messages, in seconds or items processed'
  );
  $items['migrate all'] = array(
    'callback' => 'drush_migrate_all',
    'description' => 'Run the full migration process',
    'options' => $migration_options,
    'examples' => array(
      'migrate all' => 'Process all selected content sets completely',
      'migrate all --itemlimit=50' => 'Process a maximum of 50 items from each selected content set',
      'migrate all --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate all --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
  );
  $items['migrate clear'] = array(
    'callback' => 'drush_migrate_clear',
    'description' => 'Clear migrated data for the specified content set',
    'options' => $migration_options,
    'arguments' => array(
      'content_set' => 'Name or mcsid of content set to clear',
    ),
    'examples' => array(
      'migrate clear 4' => 'Clear the content set with mcsid 4',
      'migrate clear Articles --itemlimit=50' =>
        'Clear up to 50 items from the content set named Articles',
      'migrate clear Users --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate clear 2 --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
  );
  $items['migrate import'] = array(
    'callback' => 'drush_migrate_import',
    'description' => 'Clear imported data for the specified content set',
    'options' => $migration_options,
    'arguments' => array(
      'content_set' => 'Name or mcsid of content set to import',
    ),
    'examples' => array(
      'migrate import 4' => 'Import the content set with mcsid 4',
      'migrate import Articles --itemlimit=50' =>
        'Import up to 50 items from the content set named Articles',
      'migrate import Users --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate import 2 --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
  );
  return $items;
}

function drush_migrate_all() {
  $messages = array();
  $options = array();
  $itemlimit = drush_get_option('itemlimit');
  if ($itemlimit) {
    $options['itemlimit'] = $itemlimit;
  }
  $options['feedback'] = array(
    'function' => 'drush_print',
  );
  $feedback = drush_get_option('feedback');
  if ($feedback) {
    $parts = explode(' ', $feedback);
    $options['feedback']['frequency'] = $parts[0];
    $options['feedback']['frequency_unit'] = $parts[1];
  }
  migrate_content_process_all($messages, $options);
  foreach ($messages as $message) {
    drush_print($message);
  }
}

function drush_migrate_clear($content_set) {
  if (is_numeric($content_set)) {
    $mcsid = db_result(db_query("SELECT mcsid FROM {migrate_content_sets}
                                 WHERE mcsid=%d", $content_set));
  }
  else {
    $mcsid = db_result(db_query("SELECT mcsid FROM {migrate_content_sets}
                                 WHERE LOWER('%s') = LOWER(description)", $content_set));
  }
  $messages = array();
  $options = array();
  $itemlimit = drush_get_option('itemlimit');
  if ($itemlimit) {
    $options['itemlimit'] = $itemlimit;
  }
  $options['feedback'] = array(
    'function' => 'drush_print',
  );
  $feedback = drush_get_option('feedback');
  if ($feedback) {
    $parts = explode(' ', $feedback);
    $options['feedback']['frequency'] = $parts[0];
    $options['feedback']['frequency_unit'] = $parts[1];
  }
  migrate_content_process_clear($mcsid, $messages, $options);
  foreach ($messages as $message) {
    drush_print($message);
  }
}

function drush_migrate_import($content_set) {
  if (is_numeric($content_set)) {
    $mcsid = db_result(db_query("SELECT mcsid FROM {migrate_content_sets}
                                 WHERE mcsid=%d", $content_set));
  }
  else {
    $mcsid = db_result(db_query("SELECT mcsid FROM {migrate_content_sets}
                                 WHERE LOWER('%s') = LOWER(description)", $content_set));
  }
  $messages = array();
  $options = array();
  $itemlimit = drush_get_option('itemlimit');
  if ($itemlimit) {
    $options['itemlimit'] = $itemlimit;
  }
  $options['feedback'] = array(
    'function' => 'drush_print',
  );
  $feedback = drush_get_option('feedback');
  if ($feedback) {
    $parts = explode(' ', $feedback);
    $options['feedback']['frequency'] = $parts[0];
    $options['feedback']['frequency_unit'] = $parts[1];
  }
  migrate_content_process_import($mcsid, $messages, $options);
  foreach ($messages as $message) {
    drush_print($message);
  }
}
