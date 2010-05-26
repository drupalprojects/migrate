<?php
// $Id$

/**
 * @file
 * Define base for migration sources.
 */

/**
 * Abstract base class for source handling.
 *
 * Derived classes are expected to define __toString(), returning a string
 * describing the source and significant options. See
 * MigrateSourceSQL for an example.
 */
abstract class MigrateSource {
  /**
   * A derived source class should set this to TRUE if it is capable of
   * filtering out already-migrated items in its fetch() method (in particular,
   * if it can join directly to the map and message tables).
   *
   * @var boolean
   */
  protected $handlesMapFiltering = FALSE;
  public function handlesMapFiltering() {
    return $this->handlesMapFiltering;
  }

  /**
   * Schema field definition(s) corresponding to the primary key of the source
   * being implemented. These are used to construct the source key fields
   * of the map and message tables for a migration using this source.
   *
   * @var array
   */
  protected $keySchema = array();
  public function getKeySchema() {
    return $this->keySchema;
  }

  /**
   * Derived classes must implement fields(), returning a list of available
   * source fields.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
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
