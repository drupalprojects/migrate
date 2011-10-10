<?php

/**
 * @file
 * Drupal 7 functions 'n' things implemented in Drupal 6, to ease backporting
 */

define('LANGUAGE_NONE', 'und');

function node_type_set_defaults($info) {
  return (object)_node_type_set_defaults($info);
}

function node_type_load($type) {
  static $node_types;
  if (!isset($node_types)) {
    $node_types = node_get_types();
  }
  return $node_types[$type];
}

function taxonomy_vocabulary_save(&$vocabulary) {
  if (!is_array($vocabulary)) {
    $vocabulary = (array)$vocabulary;
  }
  return taxonomy_save_vocabulary($vocabulary);
}

function taxonomy_term_load($tid) {
  return (object)taxonomy_get_term($tid);
}

function user_delete_multiple($uids) {
  foreach ($uids as $uid) {
    user_delete(array(), $uid);
  }
}

function comment_delete_multiple($cids) {
  require_once drupal_get_path('module', 'comment') . '/comment.admin.inc';
  foreach ($cids as $cid) {
    comment_delete($cid);
  }
}

function filter_fallback_format() {
  return FILTER_FORMAT_DEFAULT;
}

/**
 * Decode an exception, especially to retrieve the correct caller.
 *
 * @param $exception
 *   The exception object that was thrown.
 * @return An error in the format expected by _drupal_log_error().
 */
function migrate_decode_exception($exception) {
  $message = $exception->getMessage();

  $backtrace = $exception->getTrace();
  // Add the line throwing the exception to the backtrace.
  array_unshift($backtrace, array('line' => $exception->getLine(), 'file' => $exception->getFile()));

  // For PDOException errors, we try to return the initial caller,
  // skipping internal functions of the database layer.
  if (is_a($exception, 'PDOException')) {
    // The first element in the stack is the call, the second element gives us the caller.
    // We skip calls that occurred in one of the classes of the database layer
    // or in one of its global functions.
//    $db_functions = array('db_query', 'pager_query', 'db_query_range', 'db_query_temporary', 'update_sql');
    $db_functions = array('db_query', '_db_query', 'pager_query', 'db_query_range', 'db_query_temporary', 'update_sql');
    while (!empty($backtrace[1]) && ($caller = $backtrace[1]) &&
        ((isset($caller['class']) && (strpos($caller['class'], 'Query') !== FALSE || strpos($caller['class'], 'Database') !== FALSE || strpos($caller['class'], 'PDO') !== FALSE)) ||
        in_array($caller['function'], $db_functions))) {
      // We remove that call.
      array_shift($backtrace);
    }
    if (isset($exception->query_string, $exception->args)) {
      $message .= ": " . $exception->query_string . "; " . print_r($exception->args, TRUE);
    }
  }
  $caller = migrate_get_last_caller($backtrace);

  return array(
    '%type' => get_class($exception),
    '%message' => $message,
    '%function' => $caller['function'],
    '%file' => $caller['file'],
    '%line' => $caller['line'],
  );
}

/**
 * Gets the last caller from a backtrace.
 *
 * @param $backtrace
 *   A standard PHP backtrace.
 * @return
 *   An associative array with keys 'file', 'line' and 'function'.
 */
function migrate_get_last_caller($backtrace) {
  // Errors that occur inside PHP internal functions do not generate
  // information about file and line. Ignore black listed functions.
  $blacklist = array('debug');
  while (($backtrace && !isset($backtrace[0]['line'])) ||
         (isset($backtrace[1]['function']) && in_array($backtrace[1]['function'], $blacklist))) {
    array_shift($backtrace);
  }

  // The first trace is the call itself.
  // It gives us the line and the file of the last call.
  $call = $backtrace[0];

  // The second call give us the function where the call originated.
  if (isset($backtrace[1])) {
    if (isset($backtrace[1]['class'])) {
      $call['function'] = $backtrace[1]['class'] . $backtrace[1]['type'] . $backtrace[1]['function'] . '()';
    }
    else {
      $call['function'] = $backtrace[1]['function'] . '()';
    }
  }
  else {
    $call['function'] = 'main()';
  }
  return $call;
}
