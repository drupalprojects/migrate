<?php

/**
 * @file
 * Support for paths in core Drupal objects
 */

class MigratePathNodeHandler extends MigrateDestinationHandler {
  public function __construct() {
    $this->registerTypes(array('node'));
  }

  public function fields($entity_type, $bundle) {
    switch ($entity_type) {
      case 'node':
        return array('path' => t('Node: Path alias'));
    }
    return array();
  }
}
