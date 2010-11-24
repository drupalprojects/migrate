<?php
// $Id$

/**
 * @file
 * Support for processing CCK fields
 */

class MigrateFieldsNodeHandler extends MigrateDestinationHandler {
  public function __construct() {
    $this->registerTypes(array('node'));
  }

  public function fields($entity_type, $bundle) {
    $fields = array();
    if (module_exists('content')) {
      $content_info = _content_type_info();
      foreach ($content_info['content types'][$bundle]['fields'] as $machine_name => $instance) {
        $fields[$machine_name] = t('Node:') . ' ' . $instance['widget']['label'] .
          ' (' . $instance['type'] . ')';
      }
    }
    return $fields;
  }

  public function prepare(stdClass $entity, stdClass $row) {
    migrate_instrument_start('MigrateDestinationEntity->prepareFields');
    // Look for Field API fields attached to this destination and handle appropriately
    $migration = Migration::currentMigration();
    $destination = $migration->getDestination();
    $entity_type = $destination->getEntityType();
    $bundle = $destination->getBundle();
    $info = content_types($bundle);
    $instances = $info['fields'];
    foreach ($instances as $machine_name => $instance) {
      if (isset($entity->$machine_name)) {
        // Normalize to an array
        if (!is_array($entity->$machine_name)) {
          $entity->$machine_name = array($entity->$machine_name);
        }
        $entity->$machine_name = migrate_field_handler_invoke_all($entity,
          $instance, $entity->$machine_name);
      }
    }
    migrate_instrument_stop('MigrateDestinationEntity->prepareFields');
  }
}

abstract class MigrateFieldHandler extends MigrateHandler {
  // abstract function arguments(...)
  abstract public function prepare(stdClass $entity, array $instance, array $values);
}

class MigrateTextFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('text', 'text_long', 'text_with_summary'));
  }

  static function arguments($summary = NULL, $format = NULL, $language = NULL) {
    $arguments = array();
    if (!is_null($summary)) {
      $arguments['summary'] = $summary;
   }
    if (!is_null($format)) {
      $arguments['format'] = $format;
    }
    if (!is_null($language)) {
      $arguments['language'] = $language;
    }
    return $arguments;
  }

  public function prepare(stdClass $entity, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }

    $migration = Migration::currentMigration();
    $destination = $migration->getDestination();

    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $item = array();
      if (isset($arguments['summary'])) {
        $item['summary'] = $arguments['summary'];
      }
      $format = isset($arguments['format']) ?
        $arguments['format'] : $destination->getTextFormat();
      $item['format'] = $item['value_format'] = $format;
      $item['value'] = $value;

      $return[$delta] = $item;
      $delta++;
    }

    return isset($return) ? $return : NULL;
  }
}

class MigrateValueFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('value', 'list', 'list_boolean', 'list_number',
      'list_text', 'number_integer', 'number_decimal', 'number_float'));
  }

  public function prepare(stdClass $entity, array $instance, array $values) {
    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $return[$delta]['value'] = $value;
      $delta++;
    }
    if (!isset($return)) {
      $return = NULL;
    }
    return $return;
  }
}

class MigrateNodeReferenceFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('nodereference'));
  }

  public function prepare(stdClass $entity, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }

    // Setup the standard Field API array for saving.
    $delta = 0;
    $return = array();
    foreach ($values as $value) {
      // Don't save empty references
      if ($value) {
        $return[$delta]['nid'] = $value;
        $delta++;
      }
    }
    return $return;
  }
}

class MigrateUserReferenceFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('userreference'));
  }

  public function prepare(stdClass $entity, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }

    // Setup the standard Field API array for saving.
    $delta = 0;
    $return = array();
    foreach ($values as $value) {
      // Don't save empty references
      if ($value) {
        $return[$delta]['uid'] = $value;
        $delta++;
      }
    }
    return $return;
  }
}


class MigrateDateFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('date', 'datestamp', 'datetime'));
  }

  public function prepare(stdClass $entity, array $instance, array $values) {
    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      // Work from a timestamp
      $timestamp = MigrationBase::timestamp($value);
      // What does the destination field expect?
      if ($instance['type'] == 'datestamp') {
        // UNIX timestamp
        $value = $timestamp;
      }
      elseif ($instance['type'] == 'datetime') {
        // YYYY-MM-DD HH:MM:SS - prevent timezone application
        $value = format_date($timestamp, 'custom', 'Y-m-d H:i', 0);
      }
      elseif ($instance['type'] == 'date') {
        // ISO date: YYYY-MM-DDTHH:MM:SS - prevent timezone application
        $value = format_date($timestamp, 'custom', 'Y-m-dTH:i', 0);
      }
      else {
        // Should never get here
      }

      $return[$delta]['value'] = $value;
      $delta++;
    }
    if (!isset($return)) {
      $return = NULL;
    }
    return $return;
  }
}

class MigrateEmailFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('email'));
  }
  
  // Copied from ValueFieldHandler::prepare except the propname is email.
  public function prepare(stdClass $entity, array $instance, array $values) {
    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $return[$delta]['email'] = $value;
      $delta++;
    }
    if (!isset($return)) {
      $return = NULL;
    }
    return $return;
  }
}