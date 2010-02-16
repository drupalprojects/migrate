<?php
// $Id$

/**
 * @file
 * Define base for migration sources.
 */

/**
 * Abstract base class for source handling.
 */
abstract class MigrateSource {
  /**
   * Schema field definition(s) corresponding to the primary key of the source
   * being implemented. These are used to construct the source key fields
   * of the map and message tables for a migration using this source.
   *
   * @var array
   */
  public $keySchema = array();

  /**
   * Derived classes must implement fields(), returning a list of available
   * source fields.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping and
   *        addFieldMappingCallback)
   *  Values: Human-friendly descriptions of the fields.
   */
  abstract public function fields();

  /**
   * Derived classes must implement count(), returning a count of all available
   * source records. If the count is cached, it must be refreshed when TRUE is passed.
   */
  abstract public function count($refresh = FALSE);

  /**
   * Derived classes must implement fetch(), returning the next record from the
   * source as an object.
   */
  abstract public function fetch(Migration $migration);

  /**
   * Simply save the key schema.
   *
   * @param array $key_schema
   */
  public function __construct(array $key_schema) {
    $this->keySchema = $key_schema;
  }
}
