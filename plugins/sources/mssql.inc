<?php

/**
 * @file
 * Define a MigrateSource for importing from Microsoft SQL Server databases. This
 * plugin has limited capabilities compared to MigrateSourceSQL which can join
 * with the map/message tables and only fetch un-migrated content. This plugin
 * fetched everything unless an idlist or itemlimit are supplied. Note that
 * this is not completely inefficient; already migrated content does not actually
 * get re-imported, it just gets re-fetched.
 */

/**
 * Implementation of MigrateSource, to handle imports from remote DB servers.
 */
class MigrateSourceMSSQL extends MigrateSource {
  /**
   * Array containing information for connecting to SQL Server:
   *  servername - Hostname of the SQL Server
   *  username - Username to connect as
   *  password - Password for logging in
   *  database (optional) - Database to select after connecting
   *
   * @var array
   */
  protected $configuration;

  /**
   * The active MS SQL Server connection for this source.
   *
   * @var resource
   */
  protected $connection;

  /**
   * The SQL query from which to obtain data. Is a string.
   */
  protected $query;

  /**
   * The result object from executing the query - traversed to process the
   * incoming data.
   */
  protected $result;

  /**
   * Number of eligible rows processed so far (used for itemlimit checking)
   *
   * @var int
   */
  protected $numProcessed = 0;

  /**
   * By default, mssql_query fetches all results - severe memory problems with
   * big tables. So, we will fetch a batch at a time.
   *
   * @var int
   */
  protected $batchSize;

  /**
   * If the map is a MigrateMSSQLMap, and the table is compatible with the
   * source query, we can join directly to the map and make things much faster
   * and simpler.
   *
   * @todo: No MigrateMSSQL exists and probably D7 will use PDO MSSQL instead.
   *
   * @var boolean
   */
  protected $mapJoinable = FALSE;

  /**
   * Return an options array for MS SQL sourcews.
   *
   * @param int $batch_size
   *  Number of rows to pull at once (defaults to 500).
   */
  static public function options($batch_size) {
    return compact('batch_size');
  }

  /**
   * Simple initialization.
   */
  public function __construct(array $configuration, $query, $count_query,
      array $fields, array $options = array()) {
    parent::__construct();
    $this->query = $query;
    $this->countQuery = $count_query;
    $this->configuration = $configuration;
    $this->fields = $fields;
    $this->batchSize = isset($options['batch_size']) ? $options['batch_size'] : 500;
  }

  /**
   * Return a string representing the source query.
   *
   * @return string
   */
  public function __toString() {
    return (string) $this->query;
  }

  /**
   * Connect lazily to the DB server.
   */
  public function connect() {
    if (!isset($this->connection)) {
      if (isset($this->configuration['port'])) {
        $host = $this->configuration['servername'] . ':' . $this->configuration['port'];
      }
      else {
        $host = $this->configuration['servername'];
      }
      $this->connection = mssql_connect(
        $host,
        $this->configuration['username'],
        $this->configuration['password'],
        TRUE);
      if (isset($this->configuration['database'])) {
        return mssql_select_db($this->configuration['database'], $this->connection);
      }
    }
  }

  /**
   * Returns a list of fields available to be mapped from the source query.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    // The fields are defined in the Constructor for this plugin.
    return $this->fields;
  }

  /**
   * Return a count of all available source records.
   *
   * @param boolean $refresh
   *   @todo: Not implemented.
   */
  public function count($refresh = FALSE) {
    if ($this->connect()) {
      $result = mssql_query($this->countQuery);
      return reset(mssql_fetch_object($result));
    }
    else {
      return FALSE;
    }
  }

  /**
   * Implementation of Iterator::rewind() - called before beginning a foreach loop.
   */
  public function rewind() {
    $migration = Migration::currentMigration();
    $this->result = NULL;
    $this->currentRow = NULL;
    $this->numProcessed = 0;
    $this->numIgnored = 0;
    $map = $migration->getMap();

    $keys = array();
    foreach ($map->getSourceKey() as $field_name => $field_schema) {
      // Allow caller to provide an alias to table containing the primary key.
      // TODO: Alias should be determined automatically
      if (!empty($field_schema['alias'])) {
        $field_name = $field_schema['alias'] . '.' . $field_name;
      }
      $keys[] = $field_name;
    }

    /*
     * Replace :criteria placeholder with idlist or highwater clauses. We
     * considered supporting both but it is not worth the complexity. Run twice
     * instead.
     */
    $idlist = $migration->getOption('idlist');
    if (isset($idlist)) {
      // TODO: Sanitize. not critical as this is admin supplied data in drush.
      // Quote values. Works fine for numeric keys as well.
      foreach (explode(',', $idlist) as $id) {
        $quoted[] = "'$id'";
      }
      $this->query = str_replace(':criteria',
        $keys[0] . ' IN (' . implode(', ', $quoted) . ')', $this->query);
    }
    else {
      $highwaterField = $migration->getHighwaterField();
      if (isset($highwaterField['name']) && $highwater = $migration->getHighwater()) {
        if (empty($highwaterField['alias'])) {
          $highwater_name = $highwaterField['name'];
        }
        else {
          $highwater_name = $highwaterField['alias'] . '.' . $highwaterField['name'];
        }
        $this->query = str_replace(':criteria', "$highwater_name > '$highwater'", $this->query);
      }
      else {
        // No idlist or highwater. Replace :criteria placeholder with harmless WHERE
        // clause instead of empty since we don't know if an AND follows.
        $this->query = str_replace(':criteria', '1=1', $this->query);
      }
    }

    // @todo: can't do this when not map is non joinable.
    // Assume query starts with SELECT.
    // if ($itemlimit = $this->migration->getOption('itemlimit')) {
    //  $this->query = substr_replace($this->query, "SELECT TOP $itemlimit", 0, 6);
    // }

    migrate_instrument_start('mssql_query');
    $this->connect();
    $migration->showMessage($this->query, 'debug');
    $this->result = mssql_query($this->query, $this->connection, $this->batchSize);
    migrate_instrument_stop('mssql_query');

    $this->next();
  }

  /**
   * Implementation of Iterator::current() - called when entering a loop
   * iteration, returning the current row
   */
  public function current() {
    return $this->currentRow;
  }

  /**
   * Implementation of Iterator::key - called when entering a loop iteration, returning
   * the key of the current row. It must be a scalar - we will serialize
   * to fulfill the requirement, but using getCurrentKey() is preferable.
   *
   * @todo: sql.inc does not implement this method.
   */
  public function key() {
    return serialize($this->currentKey);
  }

  /**
   * Implementation of Iterator::next() - called at the bottom of the loop implicitly,
   * as well as explicitly from rewind().
   */
  public function next() {
    $this->currentRow = NULL;
    $this->currentKey = NULL;
    $migration = Migration::currentMigration();

    // If we couldn't add the itemlimit to the query directly, enforce it here
    if (!$this->mapJoinable) {
      $itemlimit = $migration->getOption('itemlimit');
      if ($itemlimit && $this->numProcessed >= $itemlimit) {
        return;
      }
    }

    // get next row
    migrate_instrument_start('MigrateSourceMSSQL next');
    $map = $migration->getMap();
    while (TRUE) {
      $this->currentRow = mssql_fetch_object($this->result);
      if (!is_object($this->currentRow)) {
        // Might be totally out of data, or just out of this batch - request another batch and see.
        migrate_instrument_start('mssql_fetch_batch');
        mssql_fetch_batch($this->result);
        migrate_instrument_stop('mssql_fetch_batch');
        if (!$this->currentRow = mssql_fetch_object($this->result)) {
          break;
        }
      }

      foreach ($map->getSourceKey() as $field_name => $field_schema) {
        $this->currentKey[$field_name] = $this->currentRow->$field_name;
      }
      if (!$this->mapJoinable) {
        // Check the map - if it's already mapped, and not marked for update, skip it
        $map_row = $migration->getMap()->getRowBySource($this->currentKey);
        if ($map_row && $map_row['needs_update'] == 0) {
          continue;
        }
        // Add map info to the row, if present
        if ($map_row) {
          foreach ($map_row as $field => $value) {
            $field = 'migrate_map_' . $field;
            $this->currentRow->$field = $value;
          }
        }
      }

      // Add some debugging, just for the first row.
      if (empty($this->numProcessed)) {
        $migration->showMessage(print_r($this->currentRow, TRUE));
      }

      // Allow the Migration to prepare this row. prepareRow() can return boolean
      // FALSE to stop processing this row. To add/modify fields on the
      // result, modify $row by reference.
      $return = TRUE;
      if (method_exists($migration, 'prepareRow')) {
        $return = $migration->prepareRow($this->currentRow);
      }

      if ($return !== FALSE) {
        $this->numProcessed++;
        break;
      }
    }

    if (!is_object($this->currentRow)) {
      $this->currentRow = NULL;
    }
    migrate_instrument_stop('MigrateSourceMSSQL next');
  }

  /**
   * Implementation of Iterator::valid() - called at the top of the loop, returning
   * TRUE to process the loop and FALSE to terminate it
   */
  public function valid() {
    return !is_null($this->currentRow);
  }
}
