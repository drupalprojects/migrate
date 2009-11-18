<?php
// $Id$

/**
 * @file
 * Drush support for the migrate module
 */

/**
 * Implementation of hook_drush_help().
 */
function migrate_drush_help($section) {
  switch ($section) {
    case 'drush:migrate all':
      return dt('Run the full migration process');
    case 'drush:migrate clear':
      return dt('Clear a given migration content set');
    case 'drush:migrate import':
      return dt('Import a given migration content set');
    case 'drush:migrate status':
      return dt('List all content sets with current status');
    case 'drush:migrate wipe':
      return dt('Delete all nodes from specified content types.');
  }
}

/**
 * Implementation of hook_drush_command().
 */
function migrate_drush_command() {
  $migration_options = array(
    '--itemlimit' => 'The maximum number of items to migrate. If unspecified, all are migrated',
    '--feedback' => 'Frequency of progress messages, in seconds or items processed',
    '--idlist' => 'A comma delimited list of ids to import or clear. If unspecified, migrate imports all pending items or clears all items for the content set.',
  );
  $items['migrate status'] = array(
    'description' => 'List all content sets with current status.',
  );
  $items['migrate all'] = array(
    'description' => 'Run the full migration process.',
    'options' => $migration_options,
    'examples' => array(
      'migrate all' => 'Process all selected content sets completely',
      'migrate all --itemlimit=50' => 'Process a maximum of 50 items from each selected content set',
      'migrate all --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate all --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
  );
  $items['migrate clear'] = array(
    'description' => 'Clear migrated data for the specified content set',
    'options' => $migration_options,
    'arguments' => array(
      'content_set' => 'Name or mcsid of content set to clear',
    ),
    'examples' => array(
      'migrate clear 4' => 'Clear the content set with mcsid 4',
      'migrate clear 4 --idlist=4,9' => 'Clear two items in mcsid 4. The ids refer to the value of the primary key in base table',
      'migrate clear Articles --itemlimit=50' =>
        'Clear up to 50 items from the content set named Articles',
      'migrate clear Users --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate clear 2 --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
  );
  $items['migrate import'] = array(
    'description' => 'Import a given migration content set',
    'options' => $migration_options,
    'arguments' => array(
      'content_set' => 'Name or mcsid of content set to import',
    ),
    'examples' => array(
      'migrate import 4' => 'Import the content set with mcsid 4',
      'migrate import 4 --idlist=4,9' => 'Import two items in mcsid 4. The ids refer to the value of the primary key in base table',
      'migrate import Articles --itemlimit=50' =>
      'Import up to 50 items from the content set named Articles',
      'migrate import Users --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate import 2 --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
  );
  $items['migrate wipe'] = array(
    'description' => 'Delete all nodes from specified content types.',
    'examples' => array(
       "drush migrate wipe story article" => 'Delete all story and article nodes.',
    ),
    'arguments' => array(
      'type' => 'A space delimited list of content type machine readable Ids.',
    ),
  );
  return $items;
}

/**
 * A simplified version of the Process (dashboard) page
 */
function drush_migrate_migrate_status() {
  $table = array();
  $table[] = array(dt('Status'), dt('Name'), dt('Total'), dt('Imported'), dt('Unimported'));
  $sql = "SELECT *
          FROM {migrate_content_sets}
          ORDER BY weight, contenttype, view_name, view_args";
  $result = db_query($sql);
  while ($row = db_fetch_object($result)) {
    if ($row->clearing) {
      $status = dt('Clearing');
    }
    elseif ($row->importing) {
      $status = dt('Importing');
    }
    elseif ($row->scanning) {
      $status = dt('Scanning');
    }
    else {
      $status = dt('');
    }
    $view = views_get_view($row->view_name);
    if (!$view) {
      drush_set_error(NULL, dt('View !view does not exist - either (re)create this view, or
        remove the content set using it.', array('!view' => $row->view_name)));
      continue;
    }
    $maptable = migrate_map_table_name($row->mcsid);
    $imported = db_result(db_query('SELECT COUNT(*) FROM {' . $maptable . '}'));
    // If not caching counts, override the saved count with a fresh count
    if (!variable_get('migrate_cache_counts', 0)) {
      $total = _migrate_get_view_count($view, $row->view_args);
    }
    else {
      $total = $row->rowcount;
    }

    $table[] = array($status, $row->description, $total, $imported, $total-$imported);
  }
  drush_print_table($table, TRUE);
}

/**
 * Process all enabled operations
 */
function drush_migrate_migrate_all() {
  drush_migrate_set_verbose();
  $messages = array();
  $options = array();
  $itemlimit = drush_get_option('itemlimit');
  if ($itemlimit) {
    $options['itemlimit'] = $itemlimit;
  }
  $options['feedback'] = array(
    'function' => 'drush_log',
  );
  $feedback = drush_get_option('feedback');
  if ($feedback) {
    $parts = explode(' ', $feedback);
    $options['feedback']['frequency'] = $parts[0];
    $options['feedback']['frequency_unit'] = drupal_strtolower($parts[1]);
    if ($options['feedback']['frequency_unit'] != 'seconds' &&
        $options['feedback']['frequency_unit'] != 'items') {
      drush_set_error(NULL, dt("Invalid feedback frequency unit '!unit'",
        array('!unit' => $options['feedback']['frequency_unit'])));
      return;
    }
  }
  migrate_content_process_all($messages, $options);
  foreach ($messages as $message) {
    drush_log($message);
  }
}

/**
 * Clear one specified content set
 *
 * @param $content_set
 *  The mcsid or the name of the content set
 */
function drush_migrate_migrate_clear($content_set) {
  drush_migrate_set_verbose();
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
  if ($idlist = drush_get_option('idlist', FALSE)) {
    $options['idlist'] = $idlist;
  }
  if ($itemlimit = drush_get_option('itemlimit', FALSE)) {
    $options['itemlimit'] = $itemlimit;
  }
  $options['feedback'] = array(
    'function' => 'drush_log',
  );
  $feedback = drush_get_option('feedback');
  if ($feedback) {
    $parts = explode(' ', $feedback);
    $options['feedback']['frequency'] = $parts[0];
    $options['feedback']['frequency_unit'] = $parts[1];
    if ($options['feedback']['frequency_unit'] != 'seconds' &&
        $options['feedback']['frequency_unit'] != 'items') {
      drush_set_error(NULL, dt("Invalid feedback frequency unit '!unit'",
        array('!unit' => $options['feedback']['frequency_unit'])));
      return;
    }
  }
  migrate_content_process_clear($mcsid, $messages, $options);
  foreach ($messages as $message) {
    drush_log($message);
  }
}

/**
 * Import one specified content set
 *
 * @param $content_set
 *  The mcsid or the name of the content set
 */
function drush_migrate_migrate_import($content_set) {
  drush_migrate_set_verbose();
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
  if ($idlist = drush_get_option('idlist', FALSE)) {
    $options['idlist'] = $idlist;
  }
  if ($itemlimit = drush_get_option('itemlimit', FALSE)) {
    $options['itemlimit'] = $itemlimit;
  }
  $options['feedback'] = array(
    'function' => 'drush_log',
  );
  $feedback = drush_get_option('feedback');
  if ($feedback) {
    $parts = explode(' ', $feedback);
    $options['feedback']['frequency'] = $parts[0];
    $options['feedback']['frequency_unit'] = $parts[1];
    if ($options['feedback']['frequency_unit'] != 'seconds' &&
        $options['feedback']['frequency_unit'] != 'items') {
      drush_set_error(NULL, dt("Invalid feedback frequency unit '!unit'",
        array('!unit' => $options['feedback']['frequency_unit'])));
      return;
    }
  }
  migrate_content_process_import($mcsid, $messages, $options);
  foreach ($messages as $message) {
    drush_log($message);
  }
}

/**
 * A drush command callback.
 */
function drush_migrate_migrate_wipe() {
  $types = func_get_args();
  
  if (!drush_confirm(dt('You are about to delete all nodes from the @types content type(s). Do you want to continue?', array('@types' => implode(', ', $types))))) {
    return drush_set_error('Aborting.');
  }
  
  $placeholders = db_placeholders($types, 'varchar');
  $sql = "SELECT nid FROM {node} WHERE type IN ($placeholders)";
  $result = db_query($sql, $types);
  while ($row = db_fetch_object($result)) {
    node_delete($row->nid);

    // Quickly wipe node_load() cache to avoid memory bloat.
    node_load('invalid_condition', NULL, TRUE);
    
    // Only shows for verbose level.
    drush_log(dt('Memory usage is @mem MB', array('@mem' => round(memory_get_usage() / 1048576, 2))));
  }
}

/**
 * Set the verbose context if 'feedback' is requested.
 */
function drush_migrate_set_verbose() {
  if (drush_get_option('feedback')) {
    drush_set_context('DRUSH_VERBOSE',  TRUE);
  }
}
