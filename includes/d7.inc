<?php
// $Id$

/**
 * @file
 * Drupal 7 functions implemented in Drupal 6, to ease backporting
 */

define('LANGUAGE_NONE', 'und');

function node_type_set_defaults($info) {
  return (object)_node_type_set_defaults($info);
}

function taxonomy_vocabulary_save(&$vocabulary) {
  if (!is_array($vocabulary)) {
    $vocabulary = (array)$vocabulary;
  }
  return taxonomy_save_vocabulary($vocabulary);
}
