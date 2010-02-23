<?php
// $Id$

/**
 * @file
 * Defines base for migration destinations.
 */

/**
 * Abstract base class for destination handling.
 *
 * Derived classes are expected to define __toString(), returning a string
 * describing the type of destination and significant options. See
 * MigrateDestinationEntity for an example.
 */
abstract class MigrateDestination {
  /**
   * Schema field definition(s) corresponding to the primary key of the destination
   * being implemented. These are used to construct the destination key fields
   * of the map table for a migration using this destination.
   *
   * @var array
   */
  public $keySchema = array();

  /**
   * Derived classes should set this to TRUE in their constructor if implementing
   * bulkRollback(). TODO: Should we just do methodExists on bulkRollback?
   *
   * @var boolean
   */
  public $supportsBulkRollback = FALSE;

  /**
   * Derived classes must implement fields(), returning a list of available
   * destination fields.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping and
   *        addFieldMappingCallback)
   *  Values: Human-friendly descriptions of the fields.
   */
  abstract public function fields();

  /**
   * Derived classes must implement either bulkRollback or rollback() according to
   * the signatures below, to rollback (usually by deletion) previously-migrated
   * items.
   *
   * $ids is an array of single-field keys to be deleted
   * abstract public function bulkRollback(Migration $migration, array $ids) {
   *
   * $key is an array of fields keying a single entry to delete
   * abstract public function rollback(Migration $migration, array $key);
   */

  /**
   * Derived classes must implement import(), to construct one new object (pre-pppulated
   * using field mappings in the Migration). It is expected to call prepare and
   * complete handlers, passing them $row (the raw data from the source).
   */
  abstract public function import(Migration $migration, stdClass $object, stdClass $row, array $arguments);

  /**
   * Simply save the key schema.
   *
   * @param array $key_schema
   */
  public function __construct(array $key_schema) {
    $this->keySchema = $key_schema;
  }
}
