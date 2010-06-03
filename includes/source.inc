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
abstract class MigrateSource implements Iterator {
  /**
   * The Migration object using this source.
   * @var Migration
   */
  protected $migration;

  /**
   * The current row from the quey
   *
   * @var stdClass
   */
  protected $currentRow;

  /**
   * The primary key of the current row
   *
   * @var array
   */
  protected $currentKey;
  public function getCurrentKey() {
    return $this->currentKey;
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
   * Simply save the migration.
   *
   * @param Migration $migration
   */
  public function __construct(Migration $migration) {
    $this->migration = $migration;
  }
}
