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
   * Map object tracking relationships between source and destination data
   *
   * @var MigrateMap
   */
  protected $map;
  public function getMap() {
    return $this->map;
  }

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
   * Record a message related to a source record
   *
   * @param string $message
   *  The message to record.
   * @param int $level
   *  Optional message severity (defaults to MESSAGE_ERROR).
   */
  public function saveMessage($message, $level = Migration::MESSAGE_ERROR) {
    if ($this->currentKey) {
      $this->map->saveMessage($this->currentKey, $message, $level);
    }
  }

  public function importedCount() {
    return $this->map->importedCount();
  }

  public function getKey() {
    return $this->map->getSourceKey();
  }

  /**
   * Simply save the key schema.
   *
   * @param array $key_schema
   */
  public function __construct(Migration $migration, MigrateMap $map) {
    $this->migration = $migration;
    $this->map = $map;
  }
}
