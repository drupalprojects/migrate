<?php
// $Id$

/**
 * @file
 * Support for processing entity fields
 */

class MigrateFieldsEntityHandler extends MigrateHandler {
  public function prepare(Migration $migration, stdClass $entity, stdClass $row) {
    migrate_instrument_start('MigrateDestinationEntity->prepareFields');
    // Look for Field API fields attached to this destination and handle appropriately
    $entity_type = $migration->destination->entityType;
    $bundle = $migration->destination->bundle;
    $instances = field_info_instances($entity_type, $bundle);
    foreach ($instances as $machine_name => $instance) {
      if (isset($entity->$machine_name)) {
        // Normalize to an array
        if (!is_array($entity->$machine_name)) {
          $entity->$machine_name = array($entity->$machine_name);
        }
        $field_info = field_info_field($machine_name);
        $entity->$machine_name = migrate_field_handler_invoke_all($migration, $entity, $field_info,
          $instance, $entity->$machine_name);
      }
    }
    migrate_instrument_stop('MigrateDestinationEntity->prepareFields');
  }
}

abstract class MigrateFieldHandler extends MigrateHandler {
  // abstract function arguments(...)
  abstract function prepare(Migration $migration, stdClass $entity, array $field_info, array $instance, array $values);
}

class MigrateTextFieldHandler extends MigrateFieldHandler {
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

  function prepare(Migration $migration, stdClass $entity, array $field_info, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }
    $language = isset($arguments['language']) ? $arguments['language'] : LANGUAGE_NONE;

    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $item = array();
      if (isset($arguments['summary'])) {
        $item['summary'] = $arguments['summary'];
      }
      if (isset($arguments['format'])) {
        $item['format'] = $item['value_format'] = $arguments['format'];
      }
      else {
        $item['format'] = filter_fallback_format();
      }
      $item['value'] = $value;

      $return[$language][$delta] = $item;
      $delta++;
    }

    return isset($return) ? $return : NULL;
  }
}

class MigrateTextLongFieldHandler extends MigrateTextFieldHandler {
}

class MigrateTextWithSummaryFieldHandler extends MigrateTextFieldHandler {
}

abstract class MigrateValueFieldHandler extends MigrateFieldHandler {
  function prepare(Migration $migration, stdClass $entity, array $field_info, array $instance, array $values) {
    $language = isset($arguments['language']) ? $arguments['language'] : LANGUAGE_NONE;

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

class MigrateListFieldHandler extends MigrateValueFieldHandler {
}
class MigrateListBooleanFieldHandler extends MigrateValueFieldHandler {
}
class MigrateListNumberFieldHandler extends MigrateValueFieldHandler {
}
class MigrateListTextFieldHandler extends MigrateValueFieldHandler {
}
class MigrateNumberIntegerFieldHandler extends MigrateValueFieldHandler {
}
class MigrateNumberDecimalFieldHandler extends MigrateValueFieldHandler {
}
class MigrateNumberFloatFieldHandler extends MigrateValueFieldHandler {
}

class MigrateTaxonomyTermReferenceFieldHandler extends MigrateFieldHandler {
  function prepare(Migration $migration, stdClass $entity, array $field_info, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }
    if (isset($arguments['source_type']) && $arguments['source_type'] == 'tid') {
      $condition_field = 'td.tid';
      // Nothing to do. We have tids already.
      $tids = $values;
    }
    else {
      $condition_field = 'td.name';
      // Cannot use taxonomy_term_load_multiple() since we have an array of names.
      // It wants singular value.
      // TODO: Restrict to the specific taxonomy
      $tids = db_select('taxonomy_term_data', 'td')
        ->fields('td', array('tid'))
        ->condition($condition_field, $values, 'IN')
        ->execute()
        ->fetchAllKeyed(0, 0);
      }
    $language = isset($arguments['language']) ? $arguments['language'] : LANGUAGE_NONE;
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

  /**
   * Prepare file data for saving as a Field API file field.
   *
   * // TODO: handle multiple values.
   *
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  function prepare(Migration $migration, stdClass $entity, array $field_info, array $instance, array $values) {
    static $fid;

    $arguments = $values['arguments'];
    unset($values['arguments']);
    $delta = 0;

    if ($arguments['source_path']) {
      $full_path = rtrim($arguments['source_path'], DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . ltrim($values[$delta], DIRECTORY_SEPARATOR);
    }
    else {
      $full_path = $values[$delta];
    }

    if (!file_exists($full_path)) {
      $migration->saveMessage(t('Source file does not exist: !path', array('!path' => $full_path)), MIGRATION::MESSAGE_ERROR);
      return;
    }

    // We must use full_path including directory part in order to guarantee a unique destination.
    $destination_dir = $field_info['settings']['uri_scheme'] . '://' . $instance['settings']['file_directory'];

    $source = (object) array(
      'uri' => $full_path,
      'uid' => isset($entity->uid) ? isset($entity->uid) : 0,
      'filename' => basename($full_path),
      'filemime' => file_get_mimetype($full_path),
      'timestamp' => REQUEST_TIME,
    );

    migrate_instrument_start('MigrateFileFieldHandler file_function');
    switch ($arguments['file_function']) {
      case 'file_copy':
        $file = file_copy($source, $destination_dir, $arguments['file_replace']);
        break;
      case 'file_move':
        $file = file_move($source, $destination_dir, $arguments['file_replace']);
        break;
      case 'file_fast':
        // Keep re-using an existing file. We still benefit from the file_exists() check above.
        if (!isset($fid)) {
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
          $file = new stdClass;
          $file->fid = $fid;
        }
        break;
    }
    migrate_instrument_stop('MigrateFileFieldHandler file_function');

    if ($file) {
      // Build up a return object.
      $object_field['fid'] = $file->fid;
      $object_field['alt'] = isset($arguments['source_alt_name']) ? $values[$arguments['source_alt_name']] : NULL;
      $object_field['title'] = isset($arguments['source_title_name']) ? $values[$arguments['source_title_name']] : NULL;
      $object_field['description'] = isset($arguments['source_description_name']) ? $values[$arguments['source_description_name']] : NULL;
      $object_field['display'] = isset($arguments['source_display_name']) ? $values[$arguments['source_display_name']] : NULL;
      $return[$arguments['language']][$delta] = $object_field;
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
   *   Value of $replace in that file function. Does not apply to file_save(). Defaults to FILE_EXISTS_RENAME.
   * @param language
   *   Language of the text (defaults to destination language)
   * @param source_alt_name
   * @param source_title_name
   * @param source_description_name
   * @param source_display_name
   *
   */
 static function arguments($source_path = NULL, $file_function = 'file_copy', $file_replace = FILE_EXISTS_RENAME, $language = LANGUAGE_NONE, $source_alt_name = NULL, $source_title_name = NULL, $source_description_name = NULL, $source_display_name = NULL) {
    return get_defined_vars();
  }
}

class MigrateImageFieldHandler extends MigrateFileFieldHandler {
}

class MigrateNodeReferenceFieldHandler extends MigrateFieldHandler {
  function prepare(Migration $migration, stdClass $entity, array $field_info, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }
    $language = isset($arguments['language']) ? $arguments['language'] : LANGUAGE_NONE;

    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $return[$language][$delta]['nid'] = $value;
      $delta++;
    }
    return $return;
  }
}
