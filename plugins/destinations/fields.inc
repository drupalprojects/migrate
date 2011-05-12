<?php

/**
 * @file
 * Support for processing entity fields
 */

class MigrateFieldsEntityHandler extends MigrateDestinationHandler {
  public function __construct() {
    $this->registerTypes(array('entity'));
  }

  public function fields($entity_type, $bundle) {
    $fields = array();
    $field_instance_info = field_info_instances($entity_type, $bundle);
    foreach ($field_instance_info as $machine_name => $instance) {
      $field_info = field_info_field($machine_name);

      $fields[$machine_name] = t('Field:') . ' ' . $instance['label'] .
        ' (' . $field_info['type'] . ')';
    }

    return $fields;
  }

  public function prepare($entity, stdClass $row) {
    migrate_instrument_start('MigrateDestinationEntity->prepareFields');
    // Look for Field API fields attached to this destination and handle appropriately
    $migration = Migration::currentMigration();
    $destination = $migration->getDestination();
    $entity_type = $destination->getEntityType();
    $bundle = $destination->getBundle();
    $instances = field_info_instances($entity_type, $bundle);
    foreach ($instances as $machine_name => $instance) {
      if (property_exists($entity, $machine_name)) {
        // Normalize to an array
        if (!is_array($entity->$machine_name)) {
          $entity->$machine_name = array($entity->$machine_name);
        }
        $field_info = field_info_field($machine_name);
        $entity->$machine_name = migrate_field_handler_invoke_all($entity, $field_info,
          $instance, $entity->$machine_name);
      }
    }
    migrate_instrument_stop('MigrateDestinationEntity->prepareFields');
  }
}

abstract class MigrateFieldHandler extends MigrateHandler {
  // abstract function arguments(...)
  abstract public function prepare($entity, array $field_info, array $instance, array $values);

  /**
   * Determine the language of the field
   *
   * @param $entity
   * @param $field_info
   * @param $arguments
   * @return string language code
   */
  function getFieldLanguage($entity, $field_info, array $arguments) {
    $migration = Migration::currentMigration();
    switch (TRUE) {
      case !field_is_translatable($migration->getDestination()->getEntityType(), $field_info):
          return LANGUAGE_NONE;
      case isset($arguments['language']):
        return $arguments['language'];
      case $entity->language != LANGUAGE_NONE:
        return $entity->language;
        break;
      default:
        return $migration->getDestination()->getLanguage();
    }
  }
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

  public function prepare($entity, array $field_info, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }

    $migration = Migration::currentMigration();
    $destination = $migration->getDestination();
    $language = $this->getFieldLanguage($entity, $field_info, $arguments);


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

      $return[$language][$delta] = $item;
      $delta++;
    }

    return isset($return) ? $return : NULL;
  }
}

class MigrateValueFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('value', 'list', 'list_boolean', 'list_integer',
      'list_float', 'list_text', 'number_integer', 'number_decimal', 'number_float'));
  }

  public function prepare($entity, array $field_info, array $instance, array $values) {
    $migration = Migration::currentMigration();
    $arguments = (isset($values['arguments']))? $values['arguments']: array();
    $language = $this->getFieldLanguage($entity, $field_info, $arguments);
    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $return[$language][$delta]['value'] = $value;
      $delta++;
    }
    if (!isset($return)) {
      $return = NULL;
    }
    return $return;
  }
}

class MigrateTaxonomyTermReferenceFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('taxonomy_term_reference'));
  }

  public function prepare($entity, array $field_info, array $instance, array $values) {
    $migration = Migration::currentMigration();
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }
    if (isset($arguments['source_type']) && $arguments['source_type'] == 'tid') {
      // Nothing to do. We have tids already.
      $tids = $values;
    }
    elseif ($values) {
      // Get the vocabulary for this term
      if (isset($field_info['settings']['allowed_values'][0]['vid'])) {
        $vid = $field_info['settings']['allowed_values'][0]['vid'];
      }
      else {
        $vocab_name = $field_info['settings']['allowed_values'][0]['vocabulary'];
        $names = taxonomy_vocabulary_get_names();
        $vid = $names[$vocab_name]->vid;
      }

      // Cannot use taxonomy_term_load_multiple() since we have an array of names.
      // It wants a singular value.
      $tids = db_select('taxonomy_term_data', 'td')
        ->fields('td', array('tid'))
        ->condition('td.name', $values, 'IN')
        ->condition('td.vid', $vid)
        ->execute()
        ->fetchAllKeyed(0, 0);
    }
    else {
      $tids = array();
    }
    $language = $this->getFieldLanguage($entity, $field_info, $arguments);
    $result = array();
    $i = 0;
    foreach ($tids as $tid) {
      $result[$language][$i] = array();
      $result[$language][$i]['tid'] = $tid;
      $i++;
    }
    return $result;
  }
}

class MigrateFileFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('file', 'image'));
  }

  /**
   * Prepare file data for saving as a Field API file field.
   *
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  public function prepare($entity, array $field_info, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }
    $migration = Migration::currentMigration();

    $language = $this->getFieldLanguage($entity, $field_info, $arguments);

    $return = array();
    foreach ($values as $delta => $value) {
      // Empty stuff is skipped
      if ($value) {
        $file_array = $this->buildFileArray($entity, $field_info, $instance, $migration, $arguments, $value);
        if (!empty($file_array)) {
          $return[$language][$delta] = $file_array;
        }
      }
    }
    return $return;
  }

  /**
   * Parses file information to create an appropriate data array.
   *
   * @param $entity
   * @param array $field_info
   * @param array $instance
   * @param $migration
   * @param $arguments
   * @param $value
   *
   * @return array
   */
  protected function buildFileArray($entity, array $field_info, array $instance, $migration, $arguments, $value) {
    static $fids;

    // Is the value a straight path, or JSON containing a set of properties?
    if ($value{0} == '{') {
      $properties = drupal_json_decode($value);
      $path = $properties['path'];
      // Properties passed in with the image override any set via arguments
      if (!empty($properties['alt'])) {
        $arguments['alt'] = $properties['alt'];
      }
      if (!empty($properties['title'])) {
        $arguments['title'] = $properties['title'];
      }
      if (!empty($properties['description'])) {
        $arguments['description'] = $properties['description'];
      }
      if (!empty($properties['display'])) {
        $arguments['display'] = $properties['display'];
      }
    }
    else {
      $path = $value;
    }

    // One can override a file_function via CLI or drushrc.php
    if ($migration->getOption('file_function')) {
      $file_function = $migration->getOption('file_function');
    }
    else {
      $file_function = $arguments['file_function'];
    }

    if ($arguments['source_path']) {
      $full_path = rtrim($arguments['source_path'], "/\\") .
        '/' . ltrim($path, "/\\");
    }
    else {
      $full_path = $path;
    }

    $remote = FALSE;

    // Check that source exists. If not, mark the entity as 'needs_update' and bail.
    // Sometimes the source file arrives later, when rsync is slower than DB.
    if (!is_file($full_path)) {
      // is_file() won't handle URLs, check to see if we have a remote file
      // (only relevant in the file_copy case)
      if ($file_function == 'file_copy') {
        $headers = @get_headers($full_path);
        if (is_array($headers) && preg_match('%^HTTP/[0-9]\.[0-9] 200 OK$%', $headers[0])) {
          $remote = TRUE;
        }
      }
      if (!$remote) {
        $migration->saveMessage(t('Source file does not exist: !path',
          array('!path' => $full_path)), Migration::MESSAGE_WARNING);
        $migration->needsUpdate = TRUE;
        return;
      }
    }

    $destination_dir = file_field_widget_uri($field_info, $instance);
    file_prepare_directory($destination_dir, FILE_CREATE_DIRECTORY);
    if ($remote) {
      $destination_file = $destination_dir . "/" . basename($full_path);
    }
    else {
      $destination_file = file_stream_wrapper_uri_normalize($destination_dir . "/" . str_replace('/', '-', $full_path));
    }

    $real_destination_file = drupal_realpath($destination_file);
    $source = (object) array(
      'uri' => $full_path,
      'uid' => isset($entity->uid) ? isset($entity->uid) : 0,
      'filename' => basename($full_path),
      'filemime' => file_get_mimetype($full_path),
      'timestamp' => REQUEST_TIME,
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
      switch ($file_function) {
        case 'file_copy':
          if ($remote) {
            $result = @copy($source->uri, $destination_file);
            if ($result) {
              $source->uri = $destination_file;
              $file = file_save($source);
            }
            else {
              $migration->saveMessage(t('Unable to copy file from !source',
                array('!source' => $source->uri)));
            }
          }
          else {
            $file = file_copy($source, $destination_dir, $arguments['file_replace']);
          }
          break;
        case 'file_move':
          // file_move() does a copy then delete which slow. So we implement our own.
          if (file_prepare_directory($destination_dir, FILE_CREATE_DIRECTORY)) {
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
              $migration->saveMessage(t('Unable to rename !source to !uri', array('!source' => $source->uri, '!uri' => $destination_file)), Migration::MESSAGE_ERROR);
              return;
            }
          }
          else {
            $migration->saveMessage(t('Unable to prepare directory !dir', array('!dir' => $destination_dir)), Migration::MESSAGE_ERROR);
            return;
          }
          break;
        case 'file_fast':
          // Keep re-using an existing file. We still benefit from the file_exists() check above.
          if (!isset($fids[$source])) {
            $full_path = DRUPAL_ROOT . '/misc/druplicon.png';
            $source = (object) array(
              'uri' => $full_path,
              'uid' => isset($entity->uid) ? isset($entity->uid) : 0,
              'filename' => basename($full_path),
              'filemime' => file_get_mimetype($full_path),
              'timestamp' => REQUEST_TIME,
            );
            $file = file_copy($source, $destination_dir, FILE_EXISTS_RENAME);
            $fid = $file->fid;
          }
          else {
            $file = new stdClass();
            $file->fid = $fids[$source];
          }
          break;
      }
      migrate_instrument_stop('MigrateFileFieldHandler file_function');
    }

    if ($file) {
      // Build up a return object.
      $object_field['fid'] = $file->fid;
      if (!empty($arguments['alt'])) {
        $object_field['alt'] = $arguments['alt'];
      }
      else {
        $object_field['alt'] = '';
      }
      if (!empty($arguments['title'])) {
        $object_field['title'] = $arguments['title'];
      }
      else {
        $object_field['title'] = '';
      }
      if (!empty($arguments['description'])) {
        $object_field['description'] = $arguments['description'];
      }
      else {
        $object_field['description'] = '';
      }
      if (!empty($arguments['display'])) {
        $object_field['display'] = $arguments['display'];
      }
      else {
        $object_field['display'] = TRUE;
      }
      return $object_field;
    }
    else {
      $migration->saveMessage(t('Unable to create file record for !path', array('!path' => $full_path)), Migration::MESSAGE_ERROR);
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
   * @param alt
   *   String to be used as the alt value for all file fields, or
   *   array('source_field' => 'source_field_name') to obtain the alt value from
   *   the query field named source_field_name.
   * @param title
   *   String to be used as the title value for all file fields, or
   *   array('source_field' => 'source_field_name') to obtain the title value from
   *   the query field named source_field_name.
   * @param description
   *   String to be used as the description value for all file fields, or
   *   array('source_field' => 'source_field_name') to obtain the description value from
   *   the query field named source_field_name.
   * @param display
   *   String to be used as the display value for all file fields, or
   *   array('source_field' => 'source_field_name') to obtain the display value from
   *   the query field named source_field_name.
   */
 static function arguments($source_path = NULL, $file_function = 'file_copy', $file_replace = FILE_EXISTS_RENAME, $language = NULL, $alt = NULL, $title = NULL, $description = NULL, $display = NULL) {
    return get_defined_vars();
  }
}

// TODO: node_reference and user_reference are contrib fields - should be moved
// to CCK, or migrate_extras
class MigrateNodeReferenceFieldHandler extends MigrateFieldHandler {
  public function __construct() {
    $this->registerTypes(array('node_reference'));
  }

  public function prepare($entity, array $field_info, array $instance, array $values) {
    $migration = Migration::currentMigration();
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }

    $language = $this->getFieldLanguage($entity, $field_info, $arguments);

    // Setup the standard Field API array for saving.
    $delta = 0;
    $return = array();
    foreach ($values as $value) {
      // Don't save empty references
      if ($value) {
        $return[$language][$delta]['nid'] = $value;
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

  public function prepare($entity, array $field_info, array $instance, array $values) {
    $migration = Migration::currentMigration();
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }
    $language = $this->getFieldLanguage($entity, $field_info, $arguments);

    // Setup the standard Field API array for saving.
    $delta = 0;
    $return = array();
    foreach ($values as $value) {
      // Don't save empty references
      if ($value) {
        $return[$language][$delta]['uid'] = $value;
        $delta++;
      }
    }
    return $return;
  }
}
