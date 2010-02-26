<?php
// $Id$

/**
 * @file
 * Defines base for migration destinations implemented as Drupal entities.
 */

/**
 * Abstract base class for entity-based destination handling. Holds common
 * Field API-related functions.
 */
abstract class MigrateDestinationEntity extends MigrateDestination {
  /**
   * The entity type (node, user, taxonomy_term, etc.) of the destination.
   *
   * @var string
   */
  public $entityType;

  /**
   * The bundle (node type, vocabulary, etc.) of the destination.
   *
   * @var string
   */
  public $bundle;

  /**
   * Default language for text fields in this destination.
   *
   * @var string
   */
  public $language;

  /**
   * Default input format for text fields in this destination.
   *
   * @var int
   */
  public $inputFormat;

  /**
   * Simply save the key schema.
   *
   * @param array $key_schema
   */
  public function __construct(array $key_schema, $entity_type, $bundle,
      $language = LANGUAGE_NONE, $input_format = NULL) {
    parent::__construct($key_schema);
    $this->entityType = $entity_type;
    $this->bundle = $bundle;
    $this->language = $language;
    if (isset($input_format)) {
      $this->inputFormat = $input_format;
    }
    else {
      $this->inputFormat = filter_fallback_format();
    }
  }

  public function __toString() {
    if ($this->entityType == $this->bundle) {
      $output = t('Type: %type', array('%type' => $this->entityType));
    }
    else {
      $output = t('Type: %type (%bundle)',
        array('%type' => $this->entityType, '%bundle' => $this->bundle));
    }
    // TODO: Non-default language, input format
    return $output;
  }

  // A FieldMappingCallback. Using specified map table, lookup destid given a sourceID
  // TODO: I think Migration class is the best place for map table lookups
  public function prepareLookup($row, $arguments) {
    static $frequent = array();

    if (!$migration = $arguments['migration']) {
      throw new Exception("Missing 'migration' argument in a FieldMappingCallback to 'prepareLookup'");
    }
    elseif (!isset($frequent[$migration])) {
      // Initialize if needed.
      $frequent[$migration] = array();
    }

    // Check for cached pairs.
    if (isset($frequent[$migration][$row->$arguments['source_field_name']])) {
      // Cache hit. Just return.
      return $frequent[$migration][$row->$arguments['source_field_name']];
    }
    else {
      // Cache miss. Query map table and cache the result.
      if ($sourceid = $row->$arguments['source_field_name']) {
        $mapTableName = 'migrate_map_' . drupal_strtolower($migration);
        if ($destid = db_query("SELECT destid1 FROM $mapTableName WHERE sourceid1 = :sourceid", array('sourceid' => $sourceid))->fetchField()) {
          // Found the destination value. Optionally cache it and return.
          $cache_size = isset($arguments['cache_size']) ? $arguments['cache_size'] : 500;
          // A cache_size of 0 disables the cache for a given migration.
          if ($cache_size) {
            $frequent[$migration] = array($sourceid => $destid) + $frequent[$migration];
            // Keep most recently used (MRU). Size is 500 items, by default.
            $frequent[$migration] = array_slice($frequent[$migration], 0, $cache_size, TRUE);
          }
          return $destid;
        }
      }
    }
  }

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
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  public function prepareFieldFile($values, $arguments) {
    // Set some defaults
    $arguments += array(
      'language' => $this->language,
      'file_function' => 'file_copy',
      'file_replace' => FILE_EXISTS_RENAME,
    );
    $delta = 0;

    if ($source_path = $values) {
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
        $return[$language][$delta] = $object_field;
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

  /**
   * Destination import callback to prepare input data for saving as a Field API text field
   *
   * @param mixed $values
   *  Single value, or array of values from the source
   * @param array $arguments
   *  summary (optional) - Field API summary (?)
   *  format (optional) - Text format (full HTML etc.)
   *  language (optional) - Language of the text (defaults to destination language)
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  public function prepareFieldText($values, array $arguments) {
    $language = isset($arguments['language']) ? $arguments['language'] : $this->language;

    // Normalize to an array.
    if (is_string($values)) {
      $values = array($values);
    }

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
        $item['format'] = $this->inputFormat;
      }
      $item['value'] = $value;

      $return[$language][$delta] = $item;
      $delta++;
    }

    return $return;
  }

  /**
   * Destination import callback to prepare input data for saving as a Field API text field
   *
   * @param mixed $values
   *  Single value, or array of values from the source
   * @param array $arguments
   *  summary (optional) - Field API summary (?)
   *  format (optional) - Text format (full HTML etc.)
   *  language (optional) - Language of the text (defaults to destination language)
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  function prepareFieldValue($values, $arguments) {
    $language = isset($arguments['language']) ? $arguments['language'] : $this->language;

    // Normalize to an array.
    if (is_string($values)) {
      $values = array($values);
    }

    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $return[$language][$delta]['value'] = $value;
      $delta++;
    }
    return $return;
  }

  /**
   * Destination import callback to prepare input data for saving as a Field API text field
   *
   * @param mixed $values
   *  Single value, or array of values from the source
   * @param array $arguments
   *  summary (optional) - Field API summary (?)
   *  format (optional) - Text format (full HTML etc.)
   *  language (optional) - Language of the text (defaults to destination language)
   * @return array
   *  Field API array suitable for inserting in the destination object.
   */
  function prepareFieldNodeRef($values, $arguments) {
    $language = isset($arguments['language']) ? $arguments['language'] : $this->language;

    // Normalize to an array.
    if (is_string($values)) {
      $values = array($values);
    }

    // Setup the standard Field API array for saving.
    $delta = 0;
    foreach ($values as $value) {
      $return[$language][$delta]['nid'] = $value;
      $delta++;
    }
    return $return;
  }

  public function prepareFields(stdClass $object, array $argumentOverrides = array()) {
    migrate_instrument_start('MigrateDestinationEntity->prepareFields');
    // Look for Field API fields and handle appropriately
    $field_info_instance = field_info_instances($this->entityType, $this->bundle);
    foreach ($field_info_instance as $machine_name => $instance) {
      if (isset($object->$machine_name)) {
        $field_info = field_info_field($machine_name);
        if (isset($argumentOverrides[$machine_name])) {
          $arguments = $argumentOverrides[$machine_name];
        }
        else {
          $arguments = array();
        }
        // see devel/field/info
        switch ($field_info['type']) {
          case 'text':
          case 'text_long':
          case 'text_with_summary':
            $object->$machine_name = $this->prepareFieldText($object->$machine_name, $arguments);
            break;
          case 'taxonomy_term_reference':
            // TODO: Factor out to function like others
            $terms = $object->$machine_name;
            if (!is_array($terms)) {
              $terms = array($terms);
            }
            // Cannot use taxonomy_term_load_multiple() since we have an array of names. It wants singular value.
            // TODO: Restrict to the specific taxonomy
            $tids = db_select('taxonomy_term_data', 'td')
                      ->fields('td', array('tid'))
                      ->condition('td.name', $terms, 'IN')
                      ->execute()
                      ->fetchAllKeyed(0, 0);
            $language = isset($arguments['language']) ? $arguments['language'] : LANGUAGE_NONE;
            $result = array();
            $i = 0;
            foreach ($tids as $tid) {
              // TODO: Example taxonomy hard-coded
              $result[$language][$i] = array();
              $result[$language][$i]['tid'] = $tid;
              $i++;
            }
            $object->$machine_name = $result;
            break;
          case 'file':
          case 'image':
            // TODO: Convert prepareFile callback to a prepareFieldImage method
            $object->$machine_name = $this->prepareFieldFile($object->$machine_name, $arguments);
            break;
          case 'list':
            $object->$machine_name = $this->prepareFieldValue($object->$machine_name, $arguments);
            break;
          // TODO: Should really be in migrate_extras
          case 'node_reference':
            $object->$machine_name = $this->prepareFieldNodeRef($object->$machine_name, $arguments);
            break;

          // TODO: Other core field types?
        }
      }
    }
    migrate_instrument_stop('MigrateDestinationEntity->prepareFields');
  }
}
