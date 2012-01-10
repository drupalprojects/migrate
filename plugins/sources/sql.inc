<?php

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
   * @var SelectQueryInterface
   */
  protected $originalQuery, $query, $countQuery;

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
   * List of available source fields.
   *
   * @var array
   */
  protected $fields = array();

  /**
   * If the map is a MigrateSQLMap, and the table is compatible with the
   * source query, we can join directly to the map and make things much faster
   * and simpler.
   *
   * @var boolean
   */
  protected $mapJoinable = FALSE;
  // Dynamically set whether the map is joinable - not really for production use,
  // this is primarily to support simpletests
  public function setMapJoinable($map_joinable) {
    $this->mapJoinable = $map_joinable;
  }

  /**
   * Whether this source is configured to use a highwater mark, and there is
   * a highwater mark present to use.
   *
   * @var boolean
   */
  protected $usingHighwater = FALSE;

  /**
   * Whether, in the current iteration, we have reached the highwater mark.
   *
   * @var boolen
   */
  protected $highwaterSeen = FALSE;

  /**
   * Return an options array for PDO sources.
   *
   * @param boolean $map_joinable
   *  Indicates whether the map table can be joined directly to the source query.
   * @param boolean $cache_counts
   *  Indicates whether to cache counts of source records.
   */
  static public function options($map_joinable, $cache_counts) {
    return compact('map_joinable', 'cache_counts');
  }

  /**
   * Simple initialization.
   *
   * @param SelectQueryInterface $query
   *  The query we are iterating over.
   * @param array $fields
   *  Optional - keys are field names, values are descriptions. Use to override
   *  the default descriptions, or to add additional source fields which the
   *  migration will add via other means (e.g., prepareRow()).
   * @param SelectQueryInterface $count_query
   *  Optional - an explicit count query, primarily used when counting the
   *  primary query is slow.
   * @param boolean $options
   *  Options applied to this source.
   */
  public function __construct(SelectQueryInterface $query, array $fields = array(),
       SelectQueryInterface $count_query = NULL, array $options = array()) {
    parent::__construct($options);
    $this->originalQuery = $query;
    $this->query = clone $query;
    $this->fields = $fields;
    if (is_null($count_query)) {
      $this->countQuery = clone $query->countQuery();
    }
    else {
      $this->countQuery = $count_query;
    }

    if (isset($options['map_joinable'])) {
      $this->mapJoinable = $options['map_joinable'];
    }
    else {
      // TODO: We want to automatically determine if the map table can be joined
      // directly to the query, but this won't work unless/until
      // http://drupal.org/node/802514 is committed, assume joinable for now
      $this->mapJoinable = TRUE;
/*      // To be able to join the map directly, it must be a PDO map on the same
      // connection, or a compatible connection
      $map = $migration->getMap();
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
    else {
      // Detect available fields
      $detection_query = clone $this->query;
      $result = $detection_query->range(0, 1)->execute();
      $row = $result->fetchAssoc();
      if (is_array($row)) {
        foreach ($row as $field_name => $field_value) {
          // Lower case, because Drupal forces lowercase on fetch
          $fields[drupal_strtolower($field_name)] = t('Example Content: ') . $field_value;
        }
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

    // Any caller-specified fields with the same names as extracted fields will
    // override them; any others will be added
    if ($this->fields) {
      $fields = $this->fields + $fields;
    }

    return $fields;
  }

  /**
   * Return a count of all available source records.
   */
  public function computeCount() {
    $count = $this->countQuery->execute()->fetchField();
    return $count;
  }

  /**
   * Implementation of Iterator::rewind() - called before beginning a foreach loop.
   */
  public function performRewind() {
    $this->result = NULL;
    $this->query = clone $this->originalQuery;

    // Get the keys, for potential use in joining map/message table, or enforcing
    // idlist
    $keys = array();
    foreach ($this->activeMap->getSourceKey() as $field_name => $field_schema) {
      if (isset($field_schema['alias'])) {
        $field_name = $field_schema['alias'] . '.' . $field_name;
      }
      $keys[] = $field_name;
    }

    // The rules for determining what conditions to add to the query are as
    // follows (applying first applicable rule)
    // 1. If idlist is provided, then only process items in that list (AND key IN (idlist))
    // 2. Otherwise, If there's no highwater mark, process items which are not in the map table, or
    //    are marked needs_update=1 in the map table
    //    a. mapJoinable: AND (key not in map OR map.needs_update=1)
    //    b. !mapJoinable: Apply map logic in next()
    // 3. If there is a highwater mark (and no idlist), process items above the highwater mark, plus any
    //      items with needs_update=1
    //    For now, we set mapJoinable=FALSE in this case, until we work out exactly
    //    how to manipulate the query.
    if ($this->idList) {
      // Only supp for single-field key
      $this->query->condition($keys[0], $this->idList, 'IN');
      $this->usingHighwater = FALSE;
      // We won't join with the map, but we want to add its data to the row later
      $this->mapJoinable = FALSE;
    }
    else {
      $this->usingHighwater = isset($this->highwaterField['name']);
      if ($this->usingHighwater) {
        $this->mapJoinable = FALSE;
      }
      if ($this->mapJoinable) {
        // Build the joins to the map and message tables. Because the source key
        // could have multiple fields, we need to build things up.
        // The logic is that we want to include all source rows which have no
        // existing map or message table entries, or which have map table entry
        // marked with needs_update=1. Except with highwater marks, where we don't care
        // about the existence of map/message table entries, only needs_update.
        $first = TRUE;
        $map_join = $map_condition = '';
        $count = 1;

        foreach ($this->activeMap->getSourceKey() as $field_name => $field_schema) {
          if (isset($field_schema['alias'])) {
            $field_name = $field_schema['alias'] . '.' . $field_name;
          }
          $map_key = 'sourceid' . $count++;
          if ($first) {
            $first = FALSE;
          }
          else {
            $map_join .= ' AND ';
            if (!$this->usingHighwater) {
              $map_condition .= ' OR ';
            }
          }
          $map_join .= "$field_name = map.$map_key";
          if (!$this->usingHighwater) {
            $map_condition .= "map.$map_key IS NULL";
          }
        }
        $this->query->leftJoin($this->activeMap->getMapTable(), 'map', $map_join);
        $map_condition .= ' OR map.needs_update = 1';
        $this->query->where($map_condition);

        // And as long as we have the map table, get the destination ID, the
        // import hook will need it to identify the existing destination object.
        // Alias to reduce possible collisions.
        $count = 1;
        foreach ($this->activeMap->getDestinationKey() as $field_name => $field_schema) {
          $map_key = 'destid' . $count++;
          $this->query->addField('map', $map_key, "migrate_map_$map_key");
        }
        $this->query->addField('map', 'needs_update');
        if ($this->activeMigration->getItemLimit()) {
          $this->query->range(0, $this->activeMigration->getItemLimit());
        }
      }
    }

    migrate_instrument_start('MigrateSourceSQL execute');
    $this->result = $this->query->execute();
    migrate_instrument_stop('MigrateSourceSQL execute');
  }

  /**
   * Implementation of MigrateSource::getNextRow().
   *
   * @return object
   */
  public function getNextRow() {
    return $this->result->fetchObject();
  }
}
