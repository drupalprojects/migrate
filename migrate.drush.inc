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
    case 'drush:migrate-rollback':
      return dt('Rollback the destination objects from a given migration');
    case 'drush:migrate-import':
      return dt('Perform a given migration');
    case 'drush:migrate-stop':
      return dt('Stop an active migration');
    case 'drush:migrate-status':
      return dt('List all migrations with current status');
    case 'drush:migrate-fields-destination':
      return dt('List the fields available for mapping to a destination');
    case 'drush:migrate-fields-source':
      return dt('List the fields available for mapping from a source');
    case 'drush:migrate-annotations':
      return db('View annotations for a migration and all its mappings');
    case 'drush:migrate-wipe':
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
    '--idlist' => 'A comma delimited list of ids to import or rollback. If unspecified, migrate imports all pending items or rolls back all items for the content set.',
    '--all' => 'Process all migrations',
  );
  $items['migrate-status'] = array(
    'description' => 'List all migrations with current status.',
    'options' => array(
      'refresh' => 'Recognize new migrations and update counts',
    ),
    'drupal dependencies' => array('migrate'),
  );
  $items['migrate-fields-destination'] = array(
    'description' => 'List the fields available for mapping in a destination.',
    'arguments' => array(
      'migration' => 'Name of the migration or destination class to query for fields',
      'bundle' => 'For a destination class, an optional bundle',
    ),
    'examples' => array(
      'migrate-fields-destination MyNode' => 'List fields for the destination in the MyNode migration',
      'migrate-fields-destination node article' => 'List fields available for migrating into the Article node type',
    ),
    'drupal dependencies' => array('migrate'),
  );
  $items['migrate-fields-source'] = array(
    'description' => 'List the fields available for mapping from a source.',
    'arguments' => array(
      'migration' => 'Name of the migration or destination class to query for fields',
    ),
    'examples' => array(
      'migrate-fields-destination MyNode' => 'List fields in the source query for the MyNode migration',
    ),
    'drupal dependencies' => array('migrate'),
  );
  $items['migrate-annotations'] = array(
    'description' => 'View annotations for a migration and all its mappings.',
    'arguments' => array(
      'migration' => 'Name of the migration',
    ),
    'examples' => array(
      'migrate-annotations MyNode' => 'Show annotations for the MyNode migration',
    ),
    'drupal dependencies' => array('migrate'),
  );
  $items['migrate-rollback'] = array(
    'description' => 'Roll back the destination objects from a given migration',
    'options' => $migration_options,
    // We will bootstrap to login from within the command callback.
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'arguments' => array(
      'migration' => 'Name of migration to roll back',
    ),
    'examples' => array(
      'migrate-rollback Article' => 'Roll back the article migration',
      'migrate-rollback Article --idlist=4,9' => 'Roll back two articles. The ids refer to the value of the primary key in base table',
      'migrate-rollback User --itemlimit=50' =>
        'Roll back up to 50 items from the migration named User',
      'migrate-rollback User --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate-rollback Article --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
    'drupal dependencies' => array('migrate'),
  );
  $migration_options['--update'] = 'In addition to processing unimported items from the source, update previously-imported items with new data';
  $items['migrate-import'] = array(
    'description' => 'Perform one or more migration processes',
    'options' => $migration_options,
    'arguments' => array(
      'migration' => 'Name of migration to import',
    ),
    'examples' => array(
      'migrate-import Article' => 'Import new articles',
      'migrate-import Article --update' => 'Import new items, and also update previously-imported items',
      'migrate-import Article --idlist=4,9' => 'Import two specific articles. The ids refer to the value of the primary key in base table',
      'migrate-import Article --itemlimit=50' =>
        'Import up to 50 items from the migration named Article',
      'migrate-import User --feedback="60 seconds"' => 'Display a progress message every 60 seconds or less',
      'migrate-import User --feedback="1000 items"' => 'Display a progress message every 1000 processed items or less',
    ),
    'drupal dependencies' => array('migrate'),
  );
  $items['migrate-stop'] = array(
    'description' => 'Stop an active migration operation',
    'options' => array('--all' => 'Stop all active migration operations'),
    'arguments' => array(
      'migration' => 'Name of migration to stop',
    ),
    'examples' => array(
      'migrate-stop Article' => 'Stop any active operation on the Article migration',
      'migrate-stop --all' => 'Stop all active migration operations',
    ),
    'drupal dependencies' => array('migrate'),
  );

  $items['migrate-wipe'] = array(
    'description' => 'Delete all nodes from specified content types.',
    'examples' => array(
       "migrate-wipe story article" => 'Delete all story and article nodes.',
    ),
    'arguments' => array(
      'type' => 'A space delimited list of content type machine readable Ids.',
    ),
    'drupal dependencies' => array('migrate'),
  );
  return $items;
}

// Implements hook_drush_init().
function migrate_drush_init() {
}

/**
 * Get the value of all migrate related options. Used when spawning a subshell.
 *
 * @return
 *   An array of command specific options and their values.
 */
function drush_migrate_get_options() {
  $options = array();
  $command = drush_parse_command();
  foreach ($command['options'] as $key => $value) {
    // Strip leading --
    $key = ltrim($key, '-');
    $value = drush_get_option($key);
    if (isset($value)) {
      $options[$key] = $value;
    }
  }
  return $options;
}

/*
 * Spawn a subshell which runs the same command we are currently running.
 */
function drush_migrate_backend_invoke() {
  $args = drush_get_arguments();
  $options = drush_migrate_get_options();
  // @todo: use drush_backend_invoke_args() as per http://drupal.org/node/658420.
  drush_backend_invoke(implode(' ', $args), $options);
}

/**
 * A simplified version of the dashboard page.
 */
function drush_migrate_status() {
  try {
    $refresh = drush_get_option('refresh');

    $migrations = migrate_migrations($refresh);

    $table[] = array(dt('Name'), dt('Total'), dt('Imported'), dt('Unimported'),
      dt('Status'), dt('Last imported'));
    foreach ($migrations as $migration) {
      $total = $migration->source->count($refresh);
      $imported = $migration->importedCount();
      $status = $migration->getStatus();
      switch ($status) {
        case Migration::STATUS_IDLE:
          $status = dt('Idle');
          break;
        case Migration::STATUS_IMPORTING:
          $status = dt('Importing');
          break;
        case Migration::STATUS_ROLLING_BACK:
          $status = dt('Rolling back');
          break;
        default:
          $status = dt('Unknown');
          break;
      }
      $table[] = array($migration->machineName, $total, $imported, $total-$imported, $status,
        $migration->getLastImported());
    }
    drush_print_table($table, TRUE);
    }
  catch (MigrateException $e) {
    drush_print($e->getMessage());
    exit;
  }
}

// TODO: Use drush_choice for detailed field info
function drush_migrate_fields_destination() {
  try {
    $args = func_get_args();
    $machine_name = array_shift($args);
    // First, see if it matches a migration class
    $class_name = $machine_name . 'Migration';
    if (class_exists($class_name)) {
      $migration = new $class_name;
      $destination = $migration->destination;
    }
    else {
      // No match, try a destination class
      // TODO: Client implementations won't follow this naming convention - look
      // for subclasses of MigrateDestinationEntity and match entityType?
      $class_name = 'MigrateDestination' . $machine_name;
      if (class_exists($class_name)) {
        $bundle = array_shift($args);
        $destination = new $class_name($bundle);
      }
      else {
        drush_log(dt('No class found matching !machine_name',
                     array('!machine_name' => $machine_name)),
                  'error');
        return;
      }
    }
    $table = array();
    foreach ($destination->fields() as $machine_name => $description) {
      $table[] = array($description, $machine_name);
    }
    drush_print_table($table);
  }
  catch (MigrateException $e) {
    drush_print($e->getMessage());
    exit;
  }
}

function drush_migrate_fields_source() {
  try {
    $args = func_get_args();
    $machine_name = array_shift($args);
    // See if it matches a migration class
    $class_name = $machine_name . 'Migration';
    if (class_exists($class_name)) {
      $migration = new $class_name;
      $source = $migration->source;
    }
    else {
      drush_log(dt('No class found matching !machine_name',
                   array('!machine_name' => $machine_name)),
                'error');
      return;
    }
    $table = array();
    foreach ($source->fields() as $machine_name => $description) {
      $table[] = array($description, $machine_name);
    }
    drush_print_table($table);
  }
  catch (MigrateException $e) {
    drush_print($e->getMessage());
    exit;
  }
}

function drush_migrate_annotations() {
  try {
    $args = func_get_args();
    $machine_name = array_shift($args);
    // See if it matches a migration class
    $class_name = $machine_name . 'Migration';
    if (class_exists($class_name)) {
      $migration = new $class_name;
    }
    else {
      return drush_set_error(dt('No class found matching !machine_name',
                   array('!machine_name' => $machine_name)));
    }
    drush_print($migration->annotation);
    // First group the mappings
    $annotations = array();
    foreach ($migration->fieldMappings as $mapping) {
      $annotations[$mapping->annotationGroup][] = $mapping;
    }
    // Put out each group header
    $table = array();
    $table[] = array(dt('Destination'), dt('Source'), dt('Default'),
      dt('Annotation'));
    $first = TRUE;

    foreach ($annotations as $group => $mappings) {
      if ($first) {
        $first = FALSE;
      }
      else {
        $table[] = array(' ');
      }
      // Attempt to offset and highlight the group header a bit so it stands out
      $group_header = '  -' . $group . '-';
      $table[] = array('', $group_header);
      foreach ($mappings as $mapping) {
        if (is_array($mapping->defaultValue)) {
          $default = implode(',', $mapping->defaultValue);
        }
        else {
          $default = $mapping->defaultValue;
        }
        $table[] = array($mapping->destinationField, $mapping->sourceField,
          $default, $mapping->annotationText);
      }
    }

    drush_print_table($table, TRUE);
  }
  catch (MigrateException $e) {
    drush_print($e->getMessage());
    exit;
  }
}

/**
 * Roll back one specified migration
 */
function drush_migrate_rollback() {
  try {
    $args = func_get_args();
    $machine_names = array_shift($args);
    $migrations = array();

    if (!drush_get_option('all')) {
      $names = explode(',', $machine_names);
      foreach ($names as $name) {
        $class_name = $name . 'Migration';
        $migrations[$name] = new $class_name;
      }
    }
    else {
      $migrations = array_reverse(migrate_migrations());
    }

    $options = array();
    if ($idlist = drush_get_option('idlist', FALSE)) {
      $options['idlist'] = $idlist;
    }
    if ($itemlimit = drush_get_option('itemlimit', FALSE)) {
      $options['itemlimit'] = $itemlimit;
    }
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

    foreach ($migrations as $migration) {
      drush_log(dt("Rolling back '!description' migration", array('!description' => $migration->name)));
      $return = $migration->rollback($options);
      // If it couldn't finish (presumably because it was appraoching memory_limit),
      // continue in a subprocess
      if ($return == Migration::RESULT_INCOMPLETE) {
        drush_migrate_backend_invoke();
      }
    }
  }
  catch (MigrateException $e) {
    drush_print($e->getMessage());
    exit;
  }
}

// Implement drush_hook_COMMAND_validate().
function drush_migrate_import_validate() {
  $args = func_get_args();

  if (drush_get_option('all')) {
    if (!empty($args)) {
      return drush_set_error(NULL, dt('You must specify either a migration name or --all, not both'));
    }
  }
  else {
    if (empty($args)) {
      return drush_set_error(NULL, dt('You must specify either a migration name or the -all option'));
    }
    $machine_names = explode(',', strtolower($args[0]));

    // TODO: Don't load all migrations, instantiate those passed and catch exceptions
    $migrations = migrate_migrations();
    foreach ($migrations as $migration) {
      $all[] = strtolower($migration->machineName);
    }

    if ($missing = array_diff($machine_names, $all)) {
      return drush_set_error(dt('Unrecognized migrations: ') . implode(', ', $missing));
    }
  }

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
}

/**
 * Import one specified Migration.
 *
 * @param $machine_names
 *  A comma delimited list of machine names, or the special name 'all'
 */
function drush_migrate_import() {
  try {
    $args = func_get_args();
    $machine_names = array_shift($args);
    $migrations = array();

    if (!drush_get_option('all')) {
      $names = explode(',', $machine_names);
      foreach ($names as $name) {
        $class_name = $name . 'Migration';
        $migrations[$name] = new $class_name;
      }
    }
    else {
      $migrations = migrate_migrations();
    }

    $options = array();
    if ($idlist = drush_get_option('idlist', FALSE)) {
      $options['idlist'] = $idlist;
    }
    if ($itemlimit = drush_get_option('itemlimit', FALSE)) {
      $options['itemlimit'] = $itemlimit;
    }
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

    foreach ($migrations as $migration) {
      drush_log(dt("Importing '!description' migration", array('!description' => $migration->machineName)));
      if (drush_get_option('update')) {
        $migration->prepareUpdate();
      }
      $return = $migration->import($options);
      // If it couldn't finish (presumably because it was approaching memory_limit),
      // continue in a subprocess
      if ($return == Migration::RESULT_INCOMPLETE) {
        if (drush_get_context('DRUSH_BACKEND')) {
          // Already in a subshell. Set return value and finish up.
          drush_backend_set_result($return);
          break;
        }
        else {
          // Our first incomplete migration. Just spawn subshell for the rest.
          drush_migrate_backend_invoke();
        }
      }
    }
  }
  catch (MigrateException $e) {
    drush_print($e->getMessage());
    exit;
  }
  /*
  global $_migrate_memory;
  drush_print_r($_migrate_memory);*/
}

//**
// * Stop clearing or importing a given content set.
// *
// * @param $content_set
// *  The mcsid or the name of the content set
// */
function drush_migrate_stop($content_set = NULL) {
  try {
    $args = func_get_args();
    $machine_names = array_shift($args);
    $migrations = array();

    if (!drush_get_option('all')) {
      $names = explode(',', $machine_names);
      foreach ($names as $name) {
        $class_name = $name . 'Migration';
        $migrations[$name] = new $class_name;
      }
    }
    else {
      $migrations = migrate_migrations();
    }

    foreach ($migrations as $migration) {
      drush_log(dt("Stopping '!description' migration", array('!description' => $migration->name)));
      $migration->endProcess();
    }
  }
  catch (MigrateException $e) {
    drush_print($e->getMessage());
    exit;
  }
}

/**
 * TODO: Port to d7.
 * A drush command callback.
 */
function drush_migrate_migrate_wipe() {
  // node_delete() has silly perm requirements in d5/d6.
  drush_set_option('user', 1);
  drush_bootstrap(DRUSH_BOOTSTRAP_DRUPAL_LOGIN);

  $types = func_get_args();
  $placeholders = db_placeholders($types, 'varchar');
  $sql = "SELECT nid FROM {node} WHERE type IN ($placeholders)";
  $result = db_query($sql, $types);
  while ($row = db_fetch_object($result)) {
    node_delete($row->nid);

    // Quickly wipe node_load() cache to avoid memory bloat.
    node_load('invalid_condition', NULL, TRUE);

    // Check for closeness to memory limit
    $usage = memory_get_usage();
    $memory_limit = _migrate_memory_limit();
    $pct_memory = $usage/$memory_limit;
    if ($pct_memory > MIGRATE_MEMORY_THRESHOLD) {
      // Low on memory. Spawn a subshell.
      drush_migrate_backend_invoke();
    }
  }
}
