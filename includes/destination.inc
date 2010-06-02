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
   * To support MigratePDOMap maps, derived destination classes should return
   * schema field definition(s) corresponding to the primary key of the destination
   * being implemented. These are used to construct the destination key fields
   * of the map table for a migration using this destination.
   *
   * abstract static public function getKeySchema()
   */


  /**
   * Derived classes must implement fields(), returning a list of available
   * destination fields.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  abstract public function fields();

  /**
   * Derived classes must implement either bulkRollback or rollback() according to
   * the signatures below, to rollback (usually by deletion) previously-migrated
   * items.
   *
   * $ids is an array of single-field keys to be deleted
   * abstract public function bulkRollback(Migration $migration, array $ids);
   *
   * $key is an array of fields keying a single entry to delete
   * abstract public function rollback(Migration $migration, array $key);
   */

  /**
   * Derived classes must implement import(), to construct one new object (pre-pppulated
   * using field mappings in the Migration). It is expected to call prepare and
   * complete handlers, passing them $row (the raw data from the source).
   */
  abstract public function import(Migration $migration, stdClass $object, stdClass $row);

  /**
   * Derived classes may implement preImport() and/or postImport(), to do any
   * processing they need done before or after looping over all source rows.
   * Similarly, preRollback() or postRollback() may be implemented.
   *
   * abstract public function preImport(Migration $migration);
   * abstract public function postImport(Migration $migration);
   * abstract public function preRollback(Migration $migration);
   * abstract public function postRollback(Migration $migration);
   */

  /**
   * Null constructor
   */
  public function __construct() {
  }
}

/**
 * All destination handlers should be derived from MigrateDestinationHandler
 */
abstract class MigrateDestinationHandler extends MigrateHandler {
  // abstract function arguments(...)
  abstract public function prepare(Migration $migration, stdClass $entity, stdClass $row);
}
