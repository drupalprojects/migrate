<?php
// $Id$

/**
 * @file
 * Support for paths in core Drupal objects
 */

class MigratePathNodeHandler extends MigrateHandler {
   public function prepare(Migration $migration, stdClass $node, stdClass $row) {
     if (isset($node->path_alias)) {
       $node->path = array();
       $node->path['alias'] = $node->path_alias;
       unset($node->path_alias);
     }
   }
}

class MigratePathTermHandler extends MigrateHandler {
   public function prepare(Migration $migration, stdClass $term, stdClass $row) {
     if (isset($term->path_alias)) {
       $term->path = array();
       $term->path['alias'] = $term->path_alias;
       unset($term->path_alias);
     }
   }
}
