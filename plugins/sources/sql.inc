<?php
// $Id$

/**
 * @file
 * Define a MigrateSource for importing from Drupal connections
 */

/**
 * Implementation of MigrateSource, to handle imports from Drupal connections.
 */
class MigrateSourceSQL extends MigrateSource {
  /**
   * The SQL query objects from which to obtain data, and counts of data
   *
   * @var SelectQuery
   */
  protected $query, $countQuery;

  /**
   * The result object from executing the query - traversed to process the
   * incoming data.
   *
   * @var DatabaseStatementInterface
   */
  protected $result;

  /**
   * Number of eligible rows processed so far (used for itemlimit checking)
   *
   * @var int
   */
  protected $numProcessed = 0;

  /**
   * If the map is a MigrateSQLMap, and the table is compatible with the
   * source query, we can join directly to the map and make things much faster
   * and simpler.
   *
   * @var boolean
   */
  protected $mapJoinable = FALSE;

  /**
   * Simple initialization.
   */
  public function __construct(Migration $migration, SelectQuery $query,
      SelectQuery $count_query = NULL, $map_joinable = NULL) {
    parent::__construct($migration);
    $this->query = $query;
    if (is_null($count_query)) {
      $this->countQuery = clone $query->countQuery();
    }
    else {
      $this->countQuery = $count_query;
    }

    if (!is_null($map_joinable)) {
      $this->mapJoinable = $map_joinable;
    }
    else {
      // TODO: We want to automatically determine if the map table can be joined
      // directly to the query, but this won't work unless/until
      // http://drupal.org/node/802514 is committed, assume joinable for now
      $this->mapJoinable = TRUE;
/*      // To be able to join the map directly, it must be a PDO map on the same
      // connection, or a compatible connection
      $map = $this->migration->getMap();
      if (is_a($map, 'MigrateSQLMap')) {
        $map_options = $map->getConnection()->getConnectionOptions();
        $query_options = $this->query->connection()->getConnectionOptions();

        // Identical options means it will work
        if ($map_options == $query_options) {
          $this->mapJoinable = TRUE;
        }
        else {
          // Otherwise, the one scenario we know will work is if it's MySQL and
          // the credentials match (SQLite too?)
          if ($map_options['driver'] == 'mysql' && $query_options['driver'] == 'mysql') {
            if ($map_options['host'] == $query_options['host'] &&
                $map_options['port'] == $query_options['port'] &&
                $map_options['username'] == $query_options['username'] &&
                $map_options['password'] == $query_options['password']) {
              $this->mapJoinable = TRUE;
            }
          }
        }
      }*/
    }
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
   * Returns a list of fields available to be mapped from the source query.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $fields = array();
    $queryFields = $this->query->getFields();

    if ($queryFields) {
      // Not much we can do in terms of describing the fields without manual intervention
      foreach ($queryFields as $field_name => $field_info) {
        // Lower case, because Drupal forces lowercase on fetch
        $fields[drupal_strtolower($field_name)] = drupal_strtolower(
          $field_info['table'] . '.' . $field_info['field']);
      }
    }

/*
 * Handle queries without explicit field lists
 * TODO: Waiting on http://drupal.org/node/814312
    $info = Database::getConnectionInfo($query->getConnection());
    $database = $info['default']['database'];
    foreach ($this->query->getTables() as $table) {
      if (isset($table['all_fields']) && $table['all_fields']) {

        $database = 'plants';
        $table = $table['table'];
        $sql = 'SELECT column_name
                FROM information_schema.columns
                WHERE table_schema=:database AND table_name = :table
                ORDER BY ordinal_position';
        $result = dbtng_query($sql, array(':database' => $database, ':table' => $table));
        foreach ($result as $row) {
          $fields[drupal_strtolower($row->column_name)] = drupal_strtolower(
            $table . '.' . $row->column_name);
        }
      }
    }*/
    $expressionFields = $this->query->getExpressions();
    foreach ($expressionFields as $field_name => $field_info) {
      // Lower case, because Drupal forces lowercase on fetch
      $fields[drupal_strtolower($field_name)] = drupal_strtolower($field_info['alias']);
    }

    // A Migration may manually specify source fields in ->sourceFields. This
    // allows devs to customize the description of any field, and also to add
    // fields that don't exist in the source query result (The Migration should
    // implement prepareRow() to add fields to a source result). We perform a
    // union with these manually specified fields so they are retained.
    if (isset($this->migration->sourceFields)) {
      $fields = $this->migration->sourceFields + $fields;
    }

    return $fields;
  }

  /**
   * Return a count of all available source records.
   *
   * @param boolean $refresh
   *  If TRUE, or if there is no cached count, perform a SQL COUNT query to
   *  retrieve and cache the number of rows in the query. Otherwise, return
   *  the last cached value.
   *
   *  TODO: Implement caching
   */
  public function count($refresh = FALSE) {
    $count = $this->countQuery->execute()->fetchField();
    return $count;
  }

  /**
   * Implementation of Iterator::rewind() - called before beginning a foreach loop.
   */
  public function rewind() {
    $this->result = NULL;
    $this->currentRow = NULL;
    $this->numProcessed = 0;

    // Run query, do first next()
    // TODO: Support highwater marks

    if ($this->mapJoinable) {
      // Build the joins to the map and message tables. Because the source key
      // could have multiple fields, we need to build things up.
      // The logic is that we want to include all source rows which have no
      // existing map or message table entries, or which have map table entry
      // marked with needs_update=1.
      $map = $this->migration->getMap();
      $first = TRUE;
      $map_join = $msg_join = $map_condition = $msg_condition = '';
      $count = 1;
      $keys = array();
      foreach ($map->getSourceKey() as $field_name => $field_schema) {
        if (isset($field_schema['alias'])) {
          $field_name = $field_schema['alias'] . '.' . $field_name;
        }
        $keys[] = $field_name;
        $map_key = 'sourceid' . $count++;
        if ($first) {
          $first = FALSE;
        }
        else {
          $map_join .= ' AND ';
          $msg_join .= ' AND ';
          $map_condition .= ' OR ';
          $msg_condition .= ' OR ';
        }
        $map_join .= "$field_name = map.$map_key";
        $msg_join .= "$field_name = msg.$map_key";
        $map_condition .= "map.$map_key IS NULL";
        $msg_condition .= "msg.$map_key IS NULL";
      }
      $map_condition .= ' OR map.needs_update = 1';
      $this->query->leftJoin($map->getMapTable(), 'map', $map_join);
      $this->query->leftJoin($map->getMessageTable(), 'msg', $msg_join);
      $this->query->where($map_condition);
      $this->query->where($msg_condition);
      // And as long as we have the map table, get the destination ID, the
      // import hook will need it to identify the existing destination object.
      // Alias to reduce possible collisions.
      // TODO: Handle multi-column destination keys
      $count = 1;
      foreach ($map->getDestinationKey() as $field_name => $field_schema) {
        $map_key = 'destid' . $count++;
        $this->query->addField('map', $map_key, "migrate_map_$map_key");
      }
      $this->query->addField('map', 'needs_update');
      // Only works for single-field key
      if ($this->migration->getOption('idlist')) {
        $this->query->condition($keys[0], explode(',', $this->migration->getOption('idlist')), 'IN');
      }
      if ($this->migration->getOption('itemlimit')) {
        $this->query->range(0, $this->migration->getOption('itemlimit'));
      }
    }
    migrate_instrument_start('MigrateSourceSQL execute');
    $this->result = $this->query->execute();
    migrate_instrument_stop('MigrateSourceSQL execute');

    // Load up the first row
    $this->next();
  }

  /**
   * Implementation of Iterator::next() - called at the bottom of the loop implicitly,
   * as well as explicitly from rewind().
   */
  public function next() {
    $this->currentRow = NULL;
    $this->currentKey = NULL;
    // If we couldn't add the itemlimit to the query directly, enforce it here
    if (!$this->mapJoinable) {
      $itemlimit = $this->migration->getOption('itemlimit');
      if ($itemlimit && $this->numProcessed >= $itemlimit) {
        return;
      }
    }

    // get next row
    migrate_instrument_start('MigrateSourceSQL next');
    $map = $this->migration->getMap();
    // TODO: Apply idlist etc. for non-joinable case
    while ($this->currentRow = $this->result->fetchObject()) {
      foreach ($map->getSourceKey() as $field_name => $field_schema) {
        $this->currentKey[$field_name] = $this->currentRow->$field_name;
      }
      if (!$this->mapJoinable) {
        // Check the map - if it's already mapped, and not marked for update, skip it
        $map_row = $this->migration->getMap()->getRowBySource($this->currentKey);
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

      // Allow the Migration to prepare this row. prepareRow() must return a
      // boolean TRUE for this row to get processed. To add/modify fields on the
      // result, pass $row by reference.
      if (method_exists($this->migration, 'prepareRow')) {
        $success = $this->migration->prepareRow($this->currentRow);
      }
      else {
        $success = TRUE;
      }
      if ($success) {
        $this->numProcessed++;
        break;
      }
    }
    if (!is_object($this->currentRow)) {
      $this->currentRow = NULL;
    }
    migrate_instrument_stop('MigrateSourceSQL next');
  }
}
