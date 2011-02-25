<?php

/**
 * @file
 * Copies data_row into a table using drupal_write_record()
 */

/**
 * Destination class implementing migration into a single table.
 */
class MigrateDestinationTableCopy extends MigrateDestination {
  public function __construct($tableName, $keySchema) {
    parent::__construct();
    $this->tableName = $tableName;
    $this->keySchema = $keySchema;
  }

  public function __toString() {
    $output = t('Table copy');
    return $output;
  }

  /**
   * Delete a batch of rows at once.
   *
   * @param $ids
   *  Array of IDs to be deleted.
   */
  public function bulkRollback(array $ids) {
    migrate_instrument_start('table_copy bulkRollback');
    db_delete($this->tableName)
      ->condition(key($this->keySchema), $ids, 'IN')
      ->execute();
    migrate_instrument_stop('table_copy bulkRollback');
  }

  /**
   * Import a single row.
   *
   * @param $entity
   *  Object object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields of the object that was saved if
   *  successful. FALSE on failure.
   */
  public function import(stdClass $entity, stdClass $row) {
    $migration = MigrationBase::currentMigration();

    $fields = clone $row;
    $key = key($this->keySchema);
    $valkey = $row->$key;
    unset($fields->migrate_map_destid1);
    unset($fields->needs_update);
    $query = db_merge($this->tableName)->key(array($key => $valkey))->fields((array)$fields);
    try {
      $query->execute();
      return array($valkey);
    }
    catch (MigrateException $e) {
      $migration->saveMessage($e->getMessage(), $e->getLevel());
      $migration->showMessage($e->getMessage());
    }
    catch (Exception $e) {
      $migration->saveMessage($e->getMessage());
      $migration->showMessage($e->getMessage());
    }
  }

  public function fields() {
    return array();
  }

}
