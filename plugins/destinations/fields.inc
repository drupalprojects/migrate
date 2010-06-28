<?php
// $Id$

/**
 * @file
 * Support for processing entity fields
 * TODO: Move to migrate_extras
 */

class MigrateFieldsEntityHandler extends MigrateDestinationHandler {
  public function __construct() {
    $this->registerTypes(array('entity'));
  }

  public function prepare(Migration $migration, stdClass $entity, stdClass $row) {
    migrate_instrument_start('MigrateDestinationEntity->prepareFields');
    // Look for Field API fields attached to this destination and handle appropriately
    $destination = $migration->getDestination();
    $entity_type = $destination->getEntityType();
    // Nodes only in Drupal 6
    if ($entity_type == 'node') {
      $bundle = $destination->getBundle();
      $info = content_types($bundle);
      $instances = $info['fields'];
      foreach ($instances as $machine_name => $instance) {
        if (isset($entity->$machine_name)) {
          // Normalize to an array
          if (!is_array($entity->$machine_name)) {
            $entity->$machine_name = array($entity->$machine_name);
          }
          $entity->$machine_name = migrate_field_handler_invoke_all($migration, $entity,
            $instance, $entity->$machine_name);
        }
      }
    }
    migrate_instrument_stop('MigrateDestinationEntity->prepareFields');
  }
}

abstract class MigrateFieldHandler extends MigrateHandler {
  // abstract function arguments(...)
  abstract public function prepare(Migration $migration, stdClass $entity, array $instance, array $values);
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

  public function prepare(Migration $migration, stdClass $entity, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }

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

  public function prepare(Migration $migration, stdClass $entity, array $instance, array $values) {
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

class MigrateFileFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('file', 'image'));
  }

  /**
   * Prepare file data for saving as a Field API file field.
   *
   * // TODO: handle multiple values.
   *
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  public function prepare(Migration $migration, stdClass $entity, array $instance, array $values) {
    static $fid;

    $arguments = $values['arguments'];
    unset($values['arguments']);
    $delta = 0;

    if ($arguments['source_path']) {
      $full_path = rtrim($arguments['source_path'], DIRECTORY_SEPARATOR) .
        DIRECTORY_SEPARATOR . ltrim($values[$delta], DIRECTORY_SEPARATOR);
    }
    else {
      $full_path = $values[$delta];
    }

    // Check that source exists. If not, mark the entity as 'needs_update' and bail.
    // Sometimes the source file arrives later, when rsync is slower than DB.
    if (!file_exists($full_path)) {
      $migration->saveMessage(t('Source file does not exist: !path',
        array('!path' => $full_path)), MIGRATION::MESSAGE_WARNING);
      $migration->needsUpdate = TRUE;
      return;
    }

    $destination_dir = $instance['settings']['uri_scheme'] . '://' .
      $instance['settings']['file_directory'];
    $destination_file = $destination_dir . "/" . basename($full_path);
    $real_destination_file = drupal_realpath($destination_file);

    $source = (object) array(
      'uri' => $full_path,
      'uid' => isset($entity->uid) ? isset($entity->uid) : 0,
      'filename' => basename($full_path),
      'filemime' => file_get_mimetype($full_path),
      'timestamp' => $_SERVER['REQUEST_TIME'],
    );

    // Check that destination does not exist. If it does, reuse it and return.
    if (file_exists($real_destination_file)) {
      // Save this file to DB.
      if ($existing_files = file_load_multiple(array(), array('uri' => $destination_file))) {
        // Existing record exists. Reuse it. TODO: Perhaps we never should re-use records.
        $file = reset($existing_files);
        $file = file_save($file);
      }
      else {
        // Get this orphaned file into the file table.
        $file = clone $source;
        $file->fid = NULL;
        $file->uri = $destination_file;
        $file->status |= FILE_STATUS_PERMANENT; // Save a write in file_field_presave().
        $file = file_save($file);
      }
    }
    else {
      migrate_instrument_start('MigrateFileFieldHandler file_function');
      // One can override a file_function via CLI or drushrc.php
      if ($migration->getOption('file_function')) {
        $file_function = $migration->getOption('file_function');
      }
      else {
        $file_function = $arguments['file_function'];
      }
      switch ($file_function) {
        case 'file_copy':
          $file = file_copy($source, $destination_dir, $arguments['file_replace']);
          break;
        case 'file_move':
          // file_move() does a copy then delete which slow. So we implement our own.
          if (file_prepare_directory($destination_dir)) {
            if (rename($source->uri, $real_destination_file)) {
              $file = clone $source;
              $file->fid = NULL;
              $file->uri = $destination_file;
              $file->status |= FILE_STATUS_PERMANENT; // Save a write in file_field_presave().
              $file = file_save($file);

              // Inform modules that the file has been copied.
              module_invoke_all('file_copy', $file, $source);
              break;
            }
            else {
              $migration->saveMessage(t('Unable to rename !source to !uri', array('!source' => $source->uri, '!uri' => $destination_file)), MIGRATION::MESSAGE_ERROR);
              return;
            }
          }
          else {
             $migration->saveMessage(t('Unable to prepare directory !dir', array('!dir' => $destination_dir)), MIGRATION::MESSAGE_ERROR);
             return;
          }
          break;
        case 'file_fast':
          // Keep re-using an existing file. We still benefit from the file_exists() check above.
          if (!isset($fid)) {
            $full_path = dirname($_SERVER['SCRIPT_FILENAME']) . '/misc/druplicon.png';
            $source = (object) array(
              'uri' => $full_path,
              'uid' => isset($entity->uid) ? isset($entity->uid) : 0,
              'filename' => basename($full_path),
              'filemime' => file_get_mimetype($full_path),
              'timestamp' => $_SERVER['REQUEST_TIME'],
            );
            $file = file_copy($source, $destination_dir, FILE_EXISTS_RENAME);
            $fid = $file->fid;
          }
          else {
            $file = new stdClass;
            $file->fid = $fid;
          }
          break;
      }
      migrate_instrument_stop('MigrateFileFieldHandler file_function');
    }

    if ($file) {
      // Build up a return object.
      $object_field['fid'] = $file->fid;
      $object_field['alt'] = isset($arguments['source_alt_name']) ? $values[$arguments['source_alt_name']] : NULL;
      $object_field['title'] = isset($arguments['source_title_name']) ? $values[$arguments['source_title_name']] : NULL;
      $object_field['description'] = isset($arguments['source_description_name']) ? $values[$arguments['source_description_name']] : NULL;
      $object_field['display'] = isset($arguments['source_display_name']) ? $values[$arguments['source_display_name']] : NULL;
      $return[$delta] = $object_field;
      return $return;
    }
    else {
      $migration->saveMessage(t('Unable to create file record for !path', array('!path' => $full_path)), MIGRATION::MESSAGE_ERROR);
    }
  }

  /*
   * Arguments for a file_field migration.
   *
   * @param source_path
   *   Path to source file.
   * @param file_function
   *   file_fast, file_move, or file_copy.
   * @param file_replace
   *   Value of $replace in that file function. Does not apply to file_fast(). Defaults to FILE_EXISTS_RENAME.
   * @param language
   *   Language of the text (defaults to destination language)
   * @param source_alt_name
   * @param source_title_name
   * @param source_description_name
   * @param source_display_name
   *
   */
 static function arguments($source_path = NULL, $file_function = 'file_copy',
      $file_replace = FILE_EXISTS_RENAME, $source_alt_name = NULL,
      $source_title_name = NULL, $source_description_name = NULL,
      $source_display_name = NULL) {
    return get_defined_vars();
  }
}

class MigrateNodeReferenceFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('node_reference'));
  }

  public function prepare(Migration $migration, stdClass $entity, array $instance, array $values) {
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
    $this->registerTypes(array('user_reference'));
  }

  public function prepare(Migration $migration, stdClass $entity, array $instance, array $values) {
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

  public function prepare(Migration $migration, stdClass $entity, array $instance, array $values) {
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
        // YYYY-MM-DD HH:MM:SS
        $value = format_date($timestamp, 'Y-m-d H:i');
      }
      elseif ($instance['type'] == 'date') {
        // ISO date: YYYY-MM-DDTHH:MM:SS
        $value = format_date($timestamp, 'Y-m-dTH:i');
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
