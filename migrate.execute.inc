<?php

// Add joins to map and messages tables for a given set.
function migrate_execute_add_join_tables($query, $set, $key) {
  $query->addJoin('LEFT', migrate_message_table_name($set), 'message', "$key=message.sourceid");
  $query->addJoin('LEFT', migrate_map_table_name($set), 'map', "$key=map.sourceid");
  // And as long as we have the map table, get the destination ID, the
  // import hook will need it to identify the existing destination object
    // ->addField('map', 'destid');
  // return $query;
}

function migrate_execute_add_conditions($query, $set) {
  $maptable = migrate_map_table_name($set);
  $msgtable = migrate_message_table_name($set);
  if ($idlist = drush_get_option('idlist')) {
    $query->condition(key($set['source key']), $idlist, 'IN');
  }
  if ($item_limit = drush_get_option('item_limit')) {$
    $query->range(0, $item_limit);
  }
  $query->where("map.sourceid IS NULL OR map.needs_update = 1");
  $query->where("message.sourceid IS NULL");
  return $query;
}

// Query source DB and pass each row to migrate_execute_row().
function migrate_execute_sql($query, $set) {
  $machine_name = $set['machine_name'];
  migrate_execute_add_join_tables($query, $set, 'migrate_source.' . key($set['sourcefield']));
  migrate_execute_add_conditions($query, $set);
  timer_start($machine_name . '_fetch');
  $result = $query->execute();
  timer_stop($machine_name . '_fetch');
  foreach ($result as $row) {
    $status = migrate_execute_row($row, $set);
    // All fatal statuses are negative.
    if ($status < 0) {
      break;
    }
  }
}

function migrate_execute_row($row, $set) {
  timer_start('migrate_execute_row');
  static $imported = 0;

  // Check for closeness to memory limit. If yes, redispatch.
  migrate_execute_memory();

  // Assume success until proven otherwise
  $return = MIGRATE_RESULT_COMPLETED;

  // Deliver progress report
  migrate_execute_progress();

  timer_start('import hooks');
  $errors = migrate_invoke_all('import_' . $set['destination_type'], $set, $row);
  timer_stop('import hooks');

  // Ok, we're done. Log errors to drush and to DB.
  if (!empty($errors)) {
    $success = TRUE;
    foreach ($errors as $error) {
      if (!isset($error['level'])) {
        $error['level'] = MIGRATE_MESSAGE_ERROR;
      }
      if ($error['level'] != MIGRATE_MESSAGE_INFORMATIONAL) {
        $success = FALSE;
      }
      db_insert($msgtablename)
        ->fields(array('sourceid' => $row->$sourcekey, 'level' => $error['level'], 'message' => $error['message']))
        ->execute();
    }
    if ($success) {
      $imported++;
    }
  }
  else {
    $imported++;
  }

  // more progress?
  // $message = _migrate_progress_message($lastfeedback, $imported, $description, TRUE, $return);

//  $tblinfo->status = MIGRATE_STATUS_IDLE;
//  if ($return == MIGRATE_RESULT_COMPLETED) {
//    $tblinfo->lastimported = date('Y-m-d H:i:s');
//  }
//  if (isset($feedback)) {
//    $feedback($message);
//  }

  // drupal_write_record('migrate_content_sets', $tblinfo, 'mcsid');

  timer_stop('migrate_execute_row');

  return $return;
}

function migrate_execute_progress() {
  // TODO
}

function migrate_execute_memory() {
  // TODO
}