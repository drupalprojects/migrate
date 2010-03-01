<?php
// $Id$

/**
 * @file
 * Define a MigrateSource for importing from Drupal connections
 * TODO:
 * Verify support for non-default connections - should work fine if query
 * is built on the non-default connection, but see if it joins cleanly with
 * the map/message tables on the default connection
 */

/**
 * Implementation of MigrateSource, to handle imports from Drupal connections.
 */
class MigrateSourceSQL extends MigrateSource {
  /**
   * The SQL query object from which to obtain data.
   *
   * @var SelectQuery
   */
  protected $query;

  /**
   * The result object from executing the query - traversed to process the
   * incoming data.
   *
   * @var DatabaseStatementInterface
   */
  protected $result;

  /**
   * Simple initialization.
   */
  public function __construct(SelectQuery $query, array $key_schema) {
    parent::__construct($key_schema);
    $this->query = $query;
    $this->handlesMapFiltering = TRUE;
  }

  /**
   * Return a string representing the source query.
   *
   * @return string
   */
  public function __toString() {
    return t('Query: %query', array('%query' => $this->query));
  }

  /**
   * Returns a list of fields available to be mapped from the source query.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $queryFields = $this->query->getFields();
    $fields = array();
    // Not much we can do in terms of describing the fields without manual intervention
    // TODO: Mechanism for migration developers to document source fields
    foreach ($queryFields as $field_name => $field_info) {
      // Lower case, because Drupal forces lowercase on fetch
      $fields[drupal_strtolower($field_name)] = drupal_strtolower(
        $field_info['table'] . '.' . $field_info['field']);
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
   */
  public function count($refresh = FALSE) {
    if ($refresh) {
      cache_clear_all(__METHOD__, 'cache');
    }

    $count = cache_get(__METHOD__, 'cache');
    if ($count === FALSE) {
      // Cache miss.
      $count = $this->query->countQuery()->execute()->fetchField();
      $return = cache_set(__METHOD__, $count, 'cache');
    }
    else {
      $count = $count->data;
    }
    return $count;
  }

  /**
   * Fetch the next row from the source query.
   *
   * @param Migration $migration
   *  The migration object controlling this process. Used to obtain map and
   *  message table names for filtering the query.
   * @param array $options
   *  Options for filtering the query:
   *   itemlimit - If present, process no more than this many items.
   *   idlist - If present, process only items with source keys in this
   *     comma-separated list. NOTE: Only supported for sources with single-column
   *     keys.
   */
  public function fetch(Migration $migration, array $options = array()) {
    // Do all the hard work on the first call
    if (!isset($this->result)) {
      // Build the joins to the map and message tables. Because the source key
      // could have multiple fields, we need to build things up.
      // The logic is that we want to include all source rows which have no
      // existing map or message table entries, or which have map table entry
      // marked with needs_update=1.
      $first = TRUE;
      $map_join = $msg_join = $map_condition = $msg_condition = '';
      $count = 1;
      $keys = array();
      foreach ($this->keySchema as $field_name => $field_schema) {
        // Allow caller to provide an alias to table containing the primary key.
        // TODO: Alias should be determined automatically
        if (!empty($field_schema['alias'])) {
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
      $this->query->leftJoin($migration->mapTableName, 'map', $map_join);
      $this->query->leftJoin($migration->messageTableName, 'msg', $msg_join);
      $this->query->where($map_condition);
      $this->query->where($msg_condition);
      // And as long as we have the map table, get the destination ID, the
      // import hook will need it to identify the existing destination object.
      // Alias to reduce possible collisions.
      // TODO: Handle multi-column destination keys
      $this->query->addField('map', 'destid1', 'migrate_map_destid1');
      // Only works for single-field key
      if (isset($options['idlist'])) {
        $this->query->condition($keys[0], explode(',', $options['idlist']), 'IN');
      }
      if (isset($options['itemlimit'])) {
        $this->query->range(0, $options['itemlimit']);
      }
      migrate_instrument_start('MigrateSourceSQL execute');
      $this->result = $this->query->execute();
      migrate_instrument_stop('MigrateSourceSQL execute');
    }
    migrate_instrument_start('MigrateSourceSQL fetch');
    $object = $this->result->fetchObject();
    migrate_instrument_stop('MigrateSourceSQL fetch');
    return $object;
  }
}
