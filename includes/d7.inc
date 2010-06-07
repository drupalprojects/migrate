<?php
// $Id$

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
