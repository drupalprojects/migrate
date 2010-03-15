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
        $entity->$machine_name = migrate_field_handler_invoke_all($field_info,
          $instance, $entity->$machine_name);
      }
    }
    migrate_instrument_stop('MigrateDestinationEntity->prepareFields');
  }
}

abstract class MigrateFieldHandler extends MigrateHandler {
  // abstract function arguments(...)
  abstract function prepare(array $field_info, array $instance, array $values);
}

class MigrateTextFieldHandler extends MigrateFieldHandler {
  function arguments($summary = NULL, $format = NULL, $language = NULL) {
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

  function prepare(array $field_info, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
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

    return $return;
  }
}

class MigrateTextLongFieldHandler extends MigrateTextFieldHandler {
}

class MigrateTextWithSummaryFieldHandler extends MigrateTextFieldHandler {
}

abstract class MigrateValueFieldHandler extends MigrateFieldHandler {
  function prepare(array $field_info, array $instance, array $values) {
    $language = isset($arguments['language']) ? $arguments['language'] : LANGUAGE_NONE;

    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $return[$language][$delta]['value'] = $value;
      $delta++;
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
  function prepare(array $field_info, array $instance, array $values) {
    if (isset($values['arguments'])) {
      $arguments = $values['arguments'];
      unset($values['arguments']);
    }
    else {
      $arguments = array();
    }
    if (isset($arguments['source_type']) && $arguments['source_type'] == 'tid') {
      $condition_field = 'td.tid';
    }
    else {
      $condition_field = 'td.name';
    }
    // Cannot use taxonomy_term_load_multiple() since we have an array of names.
    // It wants singular value.
    // TODO: Restrict to the specific taxonomy
    $tids = db_select('taxonomy_term_data', 'td')
      ->fields('td', array('tid'))
      ->condition($condition_field, $values, 'IN')
      ->execute()
      ->fetchAllKeyed(0, 0);
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
   * Destination import callback to prepare file data for saving as a Field API file field.
   *
   * // TODO: handle multiple values.
   *
   * @param mixed $values
   *  Single value, or array of values from the source
   * @param array $arguments
    *  file_function. file_save, File_move, or file_copy. defaults to file_copy.
    *  file_replace. Value of $replace in that file function. Does not apply to file_save(). Defaults to FILE_EXISTS_RENAME.
    *  source_field_name (required) - Name of the source field in $row
    *  source_path (optional) - Path to source file?
    *  source_alt_name (optional)
    *  title_alt_name (optional)
    *  description_alt_name (optional)
    *  display_alt_name (optional)
    *  language (optional) - Language of the text (defaults to destination language)
   * @param $destination_field_name
   *   machine_name of the field being migrated.
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  function prepare(array $field_info, array $instance, array $values) {
    $arguments = $values['arguments'];
    unset($values['arguments']);

    // Set some defaults
    $arguments += array(
      'language' => LANGUAGE_NONE,
      'file_function' => 'file_copy',
      'file_replace' => FILE_EXISTS_RENAME,
    );
    $delta = 0;

    if ($source_path = $values[$delta]) {
      // Prefix with any source_path that was passed as argument.
      if (strpos(trim($source_path, DIRECTORY_SEPARATOR), DIRECTORY_SEPARATOR) === FALSE) {
        $source_path = $arguments['source_path'] . '/' . $source_path;
      }

      $field = field_info_field($destination_field_name);
      $instance = field_info_instance($this->entityType, $destination_field_name, $this->bundle);
      $destination = $field['settings']['uri_scheme'] . '://' . $instance['settings']['file_directory'] . '/' . basename($source_path);
      $file = (object) array(
        'uri' => $source_path,
        'uid' => 1, // TODO: get this from the data or from the migration definition.
        'filename' => basename($source_path),
        'filemime' => file_get_mimetype($source_path), // TODO: verify that this is right.
        'timestamp' => REQUEST_TIME,
      );

      switch ($arguments['file_function']) {
        case 'file_copy':
          $file = file_copy($file, $destination, $arguments['file_replace']);
          break;
        case 'file_move':
          $file = file_move($file, $destination, $arguments['file_replace']);
          break;
        case 'file_save':
          $file = file_save($file);
          break;
      }

      if ($file->fid) {
        // Build up a return object.
        $object_field['fid'] = $file->fid;
        $object_field['alt'] = isset($arguments['source_alt_name']) ? $values[$arguments['source_alt_name']] : NULL;
        $object_field['title'] = isset($arguments['title_alt_name']) ? $values[$arguments['title_alt_name']] : NULL;
        $object_field['description'] = isset($arguments['description_alt_name']) ? $values[$arguments['description_alt_name']] : NULL;
        $object_field['display'] = isset($arguments['display_alt_name']) ? $values[$arguments['display_alt_name']] : NULL;
        $return[$arguments['language']][$delta] = $object_field;
        return $return;
      }
      else {
        // TODO: Maybe prepare callbacks should throw exceptions, Migration can catch them
        // and do saveMessage. Define MigrateException class...
        //$migration->saveMessage(t('...'));
        throw new Exception(t('Unable to create file record for !path', array('!path' => $source_path)));
      }
    }
  }
}

class MigrateImageFieldHandler extends MigrateFileFieldHandler {
}

class MigrateNodeReferenceFieldHandler extends MigrateFieldHandler {
  function prepare(array $field_info, array $instance, array $values) {
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
