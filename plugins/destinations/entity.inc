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
  public $textFormat;

  /**
   * Simply save the key schema.
   *
   * @param array $key_schema
   */
  public function __construct(array $key_schema, $entity_type, $bundle,
      $language = NULL, $text_format = NULL) {
    parent::__construct($key_schema);
    $this->entityType = $entity_type;
    $this->bundle = $bundle;
    $this->language = isset($language) ? $language : LANGUAGE_NONE;
    $this->textFormat = isset($text_format) ? $text_format : filter_fallback_format();
  }

  public function __toString() {
    // TODO: Link to configuration page
    if ($this->entityType == $this->bundle) {
      $output = t('%type', array('%type' => $this->entityType));
    }
    else {
      $output = t('%type (%bundle)',
        array('%type' => $this->entityType, '%bundle' => $this->bundle));
    }
    // TODO: Non-default language, input format
    return $output;
  }

  /**
   * Give handlers a shot at modifying the object before saving it.
   *
   * @param $migration
   *  Controlling migration object. Passed through to prepare handlers.
   * @param $entity
   *  Entity object to build. Prefilled with any fields mapped in the Migration.
   * @param $source_row
   *  Raw source data object - passed through to prepare handlers.
   */
  function prepare(Migration $migration, stdClass $entity, stdClass $source_row) {
    // Add source keys for debugging and identification of migrated data by hooks.
    foreach ($migration->sourceKeyMap() as $field_name => $key_name) {
      $keys[$key_name] = $source_row->$field_name;
    }
    $entity->migrate = array(
      'source_keys' => $keys,
      'machineName' => $migration->getMachineName(),
    );

    // Call any general entity handlers (in particular, the builtin field handler)
    migrate_handler_invoke_all('Entity', 'prepare', $migration, $entity, $source_row);
    // Then call any entity-specific handlers
    migrate_handler_invoke_all($this->entityType, 'prepare', $migration, $entity, $source_row);
    // Then call any prepare handler for this specific Migration.
    if (method_exists($migration, 'prepare')) {
      $migration->prepare($entity, $source_row);
    }
  }

  /**
   * Give handlers a shot at modifying the object (or taking additional action)
   * after saving it.
   *
   * @param $migration
   *  Controlling migration object. Passed through to complete handlers.
   * @param $object
   *  Entity object to build. This is the complete object after saving.
   * @param $source_row
   *  Raw source data object - passed through to complete handlers.
   */
  function complete(Migration $migration, stdClass $entity, stdClass $source_row) {
    // Call any general entity handlers (in particular, the builtin field handler)
    migrate_handler_invoke_all('Entity', 'complete', $migration, $entity, $source_row);
    // Then call any entity-specific handlers
    migrate_handler_invoke_all($this->entityType, 'complete', $migration, $entity, $source_row);
  }

}
