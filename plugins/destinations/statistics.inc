<?php
/**
* @file
* Support for node_counter statistics in core Drupal nodes
*/

class MigrateStatisticsEntityHandler extends MigrateDestinationHandler {
  public function __construct() {
    $this->registerTypes(array('node'));
  }

  public function fields() {
    if (module_exists('statistics')) {
      $fields = array(
        'totalcount' => t('Node: The total number of times the node has been viewed.'),
        'daycount' => t('Node: The total number of times the node has been viewed today.'),
        'timestamp' => t('Node: The most recent time the node has been viewed.'),
      );
    }
    else {
      $fields = array();
    }
    return $fields;
  }

  public function complete($node, stdClass $row) {
    if (module_exists('statistics') && isset($node->nid)) {
      $totalcount = $node->totalcount ? $node->totalcount : 0;
      $daycount = $node->daycount ? $node->daycount : 0;
      $timestamp = $node->timestamp ? $node->timestamp : 0;
      db_insert('node_counter')
        ->fields(array(
          'nid' => $node->nid,
          'totalcount' => $totalcount,
          'daycount' => $daycount,
          'timestamp' => $timestamp,
        ))
        ->execute();
    }
  }
}
