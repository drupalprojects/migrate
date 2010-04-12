<?php
// $Id$

/**
 * @file
 * Copies data_row into a table using drupal_write_record()
 */

/**
 * Destination class implementing migration into a single table.
 */
class MigrateDestinationTableCopy extends MigrateDestination {
  public function __construct(array $key_schema, $tableName) {
    parent::__construct($key_schema);
    $this->tableName = $tableName;
  }

  /**
   * Import a single row.
   *
   * @param $migration
   *  Controlling migration object. Passed through to prepare/complete handlers.
   * @param $entity
   *  Object object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields of the object that was saved if
   *  successful. FALSE on failure.
   */
  public function import(Migration $migration, stdClass $entity, stdClass $row) {
    $fields = clone $row;
    $key = key($this->keySchema);
    $valkey = $row->$key;
    unset($fields->migrate_map_destid1);
    $query = db_merge($this->tableName)->key(array($key => $valkey))->fields((array)$fields);
    if ($query->execute()) {
      return array($valkey);
    }
  }

  public function fields() {
    return array();
  }

}
