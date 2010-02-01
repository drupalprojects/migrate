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

// A sql callback for a content set.
function migrate_example_fetch_beer($set) {
  $fields = array_keys(migrate_example_columns_beer());
  $query = db_select('migrate_example_beer', 'migrate_source')
    ->fields('migrate_source', $fields);
  migrate_execute_sql($query, $set);
}