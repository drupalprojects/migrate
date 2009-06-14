<?php 
// $Id$

/**
 * @file
 * Implementation of destination handling
 */

/**
 * Loads the hooks for the supported modules.
 */
$path = drupal_get_path('module', 'migrate') .'/modules';
$files = drupal_system_listing('.*\.inc$', $path, 'name', 0);
foreach ($files as $module_name => $file) {
  if (module_exists($module_name)) {
    include_once($file->filename);
  }
}

function migrate_destination_invoke_all($hook, &$object, $tblinfo, $row) {
  // We could have used module_invoke_all, but unfortunately
  // module_invoke_all passes all arguments by value.
  $errors = array();
  $hook = 'migrate_destination_' . $hook;
  foreach (module_implements($hook) as $module_name) {
    $function = $module_name . '_' . $hook;
    if (function_exists($function)) {
      timer_start($function);
      $errors = array_merge($errors, (array)$function($object, $tblinfo, $row));
      timer_stop($function);
    }
  } 
  return $errors;
}

/**
 * Check if a date is valid and return the correct
 * timestamp to use. Returns -1 if the date is not
 * considered valid.
 */
function _migrate_valid_date($date) {
  //TODO: really check whether the date is valid!!
  if (empty($date)) {
    return -1;
  }

  if (is_numeric($date) && $date > -1) {
    return $date;
  }
  // strtotime() doesn't recognize dashes as separators, change to slashes
  $date = str_replace('-', '/', $date);
  
  $time = strtotime($date);
  if ($time < 0 || !$time) {
    // Handles form YYYY-MM-DD HH:MM:SS.garbage
    if (strlen($date) > 19) {
      $time = strtotime(substr($date, 0, 19));
      if ($time < 0 || !$time) {
        return -1;
      }
    }
  }
  return $time;
}

