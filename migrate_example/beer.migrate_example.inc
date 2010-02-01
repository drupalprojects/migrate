<?php

function migrate_example_columns_beer() {
  return array('bid' => 'Beer ID', 'name' => 'Name', 'manufacturer' => 'Manufacturer', 'country' => 'Country');
}

// Total number of records in this set.
function migrate_example_count_beer($set, $refresh = FALSE) {
  if ($refresh) {
   cache_clear_all(__FUNCTION__, 'cache');
 }

 $count = cache_get(__FUNCTION__, 'cache');
 if ($count === FALSE) {
  // Cache miss.
  $count = db_query('SELECT COUNT(*) FROM {migrate_example_beer}')->fetchField();
  $return = cache_set(__FUNCTION__, $count, 'cache');
 }
 else {
   $count = $count->data;
 }
 return $count;
}

// Query source DB and pass each row to migrate_execute_row().
function migrate_example_fetch_beer($set) {
  $fields = array_keys(migrate_example_columns_beer());
  $query = db_select('migrate_example_beer', 'meb')
    ->fields('meb', $fields);
  migrate_execute_add_join_tables($query, $set, 'meb.bid');
  migrate_execute_add_conditions($query, $set);
  timer_start('fetch_beer');
  $result = $query->execute();
  timer_stop('fetch_beer');
  foreach ($result as $row) {
    $status = migrate_execute_row($row, $set);
    // All fatal statuses are negative.
    if ($status < 0) {
      break;
    }
  }
}