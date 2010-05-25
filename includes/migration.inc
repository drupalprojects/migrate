<?php
// $Id$

/**
 * @file
 * The core of migration support
 * TODO:
 * Fully support compound keys in sources
 * Fully support compound keys in destinations
 * Rollback/import hooks (still useful?)
 * number_integer
 * Better cardinality handling.
 */

/**
 * The base class for all import objects. This is where most of the smarts
 * of the migrate module resides. Migrations are created by deriving from this
 * class, and in the constructor (after calling parent::__construct()) initializing
 * at a minimum the name, description, source, and destination properties. The constructor
 * will also usually make several calls to addFieldMapping().
 */
abstract class Migration extends MigrationBase {
  /**
   * Source object for the migration, derived from MigrateSource.
   *
   * @var MigrateSource
   */
  public $source;

  /**
   * Destination object for the migration, derived from MigrateDestination.
   *
   * @var MigrateDestination
   */
  public $destination;

  /**
   * Message types to be passed to saveMessage() and saved in message tables.
   * MESSAGE_INFORMATIONAL represents a condition that did not prevent the operation
   * from succeeding - all others represent different severities of conditions
   * resulting in a source record not being imported.
   */
  const MESSAGE_ERROR = 1;
  const MESSAGE_WARNING = 2;
  const MESSAGE_NOTICE = 3;
  const MESSAGE_INFORMATIONAL = 4;

  /**
   * Indicate whether the primary system of record for this migration is the
   * source, or the destination (Drupal). In the source case, migration of
   * an existing object will completely replace the Drupal object with data from
   * the source side. In the destination case, the existing Drupal object will
   * be loaded, then changes from the source applied; also, rollback will not be
   * supported.
   *
   * @var int
   */
  const SOURCE = 1;
  const DESTINATION = 2;
  public $systemOfRecord = Migration::SOURCE;

  /**
   * Specify value of needs_update for current map row. Usually set by
   * MigrateFieldHandler implementations.
   *
   * @var boolean
   */
  public $needsUpdate = FALSE;

  /**
   * Names of tables created for tracking the migration. By default they are
   * constructed from machineName, but a derived Migration class may set them
   * as it wishes.
   *
   * @var string
   */
  public $mapTableName, $messageTableName;

  /**
   * Boolean determining whether to track last_imported times in map tables
   *
   * @var boolean
   */
  protected $trackLastImported = FALSE;

  /**
   * Simple mappings between destination fields (keys) and source fields (values).
   *
   * @var array
   */
  public $fieldMappings = array();

  /**
   * The (possibly multi-column) key of the source record currently being imported.
   * @var array
   */
  protected $currSourceKey = array();

  /**
   * Mappings from field names in the source and destination objects, to the
   * corresponding field names (e.g., sourceid2, destid1, etc.) in the map and
   * message tables.
   *
   * @var array
   */
  protected $sourceKeyMap, $destinationKeyMap;

  /**
   * An array of counts. Initially used for cache hit/miss tracking.
   *
   * @var array
   */
  public $counts = array();

  /**
   * When performing a bulkRollback(), the maximum number of items to pass in
   * a single call. Can be overridden in derived class constructor.
   *
   * @var int
   */
  protected $rollbackBatchSize = 500;

  /**
   * If present, an array with keys name and alias (optional). Name refers to
   * the source columns used for tracking highwater marks. alias is an
   * optional table alias.
   *
   * @var array
   */
  public $highwaterField;

  /**
   * General initialization of a Migration object.
   */
  public function __construct() {
    parent::__construct();

    // Default generated table names - derived class can override
    $this->mapTableName = 'migrate_map_' . drupal_strtolower($this->machineName);
    $this->messageTableName = 'migrate_message_' . drupal_strtolower($this->machineName);
  }

  ////////////////////////////////////////////////////////////////////
  // Map/message table handling

  /**
   * Lazy creation of map and message tables associated with this migration.
   */
  protected function ensureTables() {
    // We use our own static to avoid a query in db_table_exists().
    static $exists = array();

    if (!isset($exists[$this->machineName]) && !db_table_exists($this->mapTableName)) {
      // Generate appropriate schema info for the map and message tables,
      // and map from the source field names to the map/msg field names
      $count = 1;
      $source_key_schema = array();
      $pks = array();
      foreach ($this->source->keySchema as $field_name => $field_schema) {
        $mapkey = 'sourceid' . $count++;
        $source_key_schema[$mapkey] = $field_schema;
        $pks[] = $mapkey;
      }

      $fields = $source_key_schema;

      // Add destination keys to map table
      $count = 1;
      foreach ($this->destination->keySchema as $field_name => $field_schema) {
        $mapkey = 'destid' . $count++;
        $fields[$mapkey] = $field_schema;
      }
      $fields['needs_update'] = array(
        'type' => 'int',
        'size' => 'tiny',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'default' => (int) FALSE,
        'description' => 'Flags existing mapped data to be updated',
      );
      if ($this->trackLastImported) {
        $fields['last_imported'] = array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'default' => 0,
          'description' => 'UNIX timestamp of the last time this row was imported',
        );
      }
      $schema = array(
        'description' => t('Mappings from source key to destination key'),
        'fields' => $fields,
        'primary key' => $pks,
      );
      db_create_table($this->mapTableName, $schema);

      // Now for the message table
      $fields = array();
      $fields['msgid'] = array(
        'type' => 'serial',
        'unsigned' => TRUE,
        'not null' => TRUE,
      );
      $fields += $source_key_schema;

      $fields['level'] = array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'default' => 1,
      );
      $fields['message'] = array(
        'type' => 'text',
        'size' => 'medium',
        'not null' => TRUE,
      );
      $schema = array(
        'description' => t('Messages generated during a migration process'),
        'fields' => $fields,
        'primary key' => array('msgid'),
        'indexes' => array('sourcekey' => $pks),
      );
      db_create_table($this->messageTableName, $schema);
    }
    else {
      // TODO: If the tables do exist, ideally we would confirm they have
      // the expected definitions, in case the migration has changed...
    }
    // Static cache so we don't do db_table_exists() again for this migration.
    $exists[$this->machineName] = $this->machineName;
  }

  /**
   * Given a (possibly multi-field) source key, return the (possibly multi-field)
   * destination key it is mapped to.
   *
   * @param array $source_id
   *  Array of source key values.
   * @return array
   *  Array of destination key values, or NULL on failure.
   */
  public function lookupDestinationID(array $source_id, Migration $destination_migration) {
    migrate_instrument_start('lookupDestinationID');
    static $frequent = array();

    // TODO: Let fieldMapping change lookup_cache_size
    $lookup_cache_size = 500;
    $frequent_key = $source_id[0];

    // Initialize if needed.
    if (!isset($frequent[$this->machineName])) {
      $frequent[$this->machineName] = array();
    }

    // Initialize if needed.
    if (!isset($destination_migration->counts['lookup_cache'][$this->machineName])) {
      $destination_migration->counts['lookup_cache'][$this->machineName] = array(
         'hit' => 0,
         'miss_hit' => 0,
         'miss_miss' => 0,
      );
    }

    // Check for cached pairs. Only works for single keys.
    if (count($source_id) == 1 && isset($frequent[$this->machineName][$frequent_key])) {
      // Cache hit. Just return.
      $destination_migration->counts['lookup_cache'][$this->machineName]['hit']++;
      return $frequent[$this->machineName][$frequent_key];
    }
    else {
      // We have a cache miss.

      // Query map table and cache the result.
      $this->ensureTables();
      $query = db_select($this->mapTableName, 'map')
                        ->fields('map', $this->destinationKeyMap());
      foreach ($this->sourceKeyMap() as $key_name) {
        $query = $query->condition("map.$key_name", array_shift($source_id), '=');
      }
      $result = $query->execute();
      $destination_id = $result->fetchAssoc();

      if ($destination_id === FALSE) {
        // We have a lookup miss.
        $destination_migration->counts['lookup_cache'][$this->machineName]['miss_miss']++;

      /*
       * Handle self references like articles that have 'related articles' and
       * users that have 'friends'. Sometimes we can't save these references because
       * the referenced item is not yet migrated. In that case, just mark this row
       * in map table as needs_update. The entity will be fully saved in a
       * subsequent import.
       */
        if ($this->machineName == $destination_migration->machineName) {
          // We have an entity that's referencing own migration.
          $destination_migration->needsUpdate = TRUE;
        }
      }
      else {
        // We have a lookup hit.
        $destination_migration->counts['lookup_cache'][$this->machineName]['miss_hit']++;

        // Optionally cache the lookup hit.
        // A lookup_cache_size of 0 would disable the cache for a given sourceMigration.
        if ($lookup_cache_size) {
          $frequent[$this->machineName] = array($frequent_key => $destination_id) + $frequent[$this->machineName];

          // Keep most recently used (MRU). Size varies between 500 and 1000 items, by default.
          if (count($frequent[$this->machineName]) > 2 * $lookup_cache_size) {
            $frequent[$this->machineName] = array_slice($frequent[$this->machineName], 0, $lookup_cache_size, TRUE);
          }
        }
      }
    }

    migrate_instrument_stop('lookupDestinationID');
    return $destination_id;
  }

  /**
   * Called upon successfully import of one record, we record a mapping from
   * the source key to the destination key. Also may be called, setting the
   * third parameter to TRUE, to signal an existing record should be remigrated.
   *
   * @param stdClass $source_row
   *  The raw source data. We use the key map derived from the source object
   *  to get the source key values.
   * @param array $dest_ids
   *  The destination key values.
   * @param boolean $needs_update
   *  Value for the needs_update field in the map. Defaults to FALSE.
   */
  public function saveIDMapping(stdClass $source_row, array $dest_ids, $needs_update = FALSE) {
    migrate_instrument_start('saveIDMapping');
    // Construct the source key
    $keys = array();
    foreach ($this->sourceKeyMap() as $field_name => $key_name) {
      $keys[$key_name] = $source_row->$field_name;
    }
    // TODO: Construct field info from destination key info (support multiple fields)
    $fields = array(
        'destid1' => $dest_ids[0],
        'needs_update' => (int)$needs_update,
    );
    if ($this->trackLastImported) {
      $fields['last_imported'] = time();
    }
    db_merge($this->mapTableName)
      ->key($keys)
      ->fields($fields)
      ->execute();
    migrate_instrument_stop('saveIDMapping');
  }

  /**
   * Record a message in the migration's message table.
   *
   * @param string $message
   *  The message to record.
   * @param int $level
   *  Optional message severity (defaults to MESSAGE_ERROR).
   */
  public function saveMessage($message, $level = Migration::MESSAGE_ERROR) {
    $fields = $this->currSourceKey;
    $fields['level'] = $level;
    $fields['message'] = $message;
    db_insert($this->messageTableName)
    ->fields($fields)
    ->execute();
  }

  public function saveHighwater($highwater) {
    db_update('migrate_status')
    ->fields(array('highwater' => $highwater))
    ->condition('machine_name', $this->machineName)
    ->execute();
  }

  /**
   * Prepares this migration to run as an update - that is, in addition to
   * unmigrated content (source records not in the map table) being imported,
   * previously-migrated content will also be updated in place.
   */
  public function prepareUpdate() {
    $this->ensureTables();
    db_update($this->mapTableName)
    ->fields(array('needs_update' => 1))
    ->execute();
  }

  ////////////////////////////////////////////////////////////////////
  // Processing

  /**
   * Add a mapping for a destination field, specifying a source field and/or
   * a default value.
   *
   * @param string $destinationField
   *  Name of the destination field.
   * @param string $sourceField
   *  Name of the source field (optional).
   */
  public function addFieldMapping($destination_field, $source_field = NULL) {
    // Warn of duplicate mappings
    if (!is_null($destination_field) && isset($this->fieldMappings[$destination_field])) {
      $outputFunction = $this->outputFunction;
      $outputFunction(t('!name addFieldMapping: !dest was previously mapped, overridden',
        array('!name' => $this->machineName, '!dest' => $destination_field)),
        'warning');
    }
    $mapping = new MigrateFieldMapping($destination_field, $source_field);
    if (is_null($destination_field)) {
      $this->fieldMappings[] = $mapping;
    }
    else {
      $this->fieldMappings[$destination_field] = $mapping;
    }
    return $mapping;
  }

  /**
   * Retrieve the last throughput for current Migration (items / minute).
   * @return integer
   */
  public function getLastThroughput() {
    $last_throughput = db_query("SELECT lastthroughput FROM {migrate_status}
                        WHERE machine_name=:machine_name",
                       array(':machine_name' => $this->machineName))
                ->fetchField();
    return $last_throughput;
  }

  /**
   * Reports whether this migration process is complete (i.e., all available
   * source rows have been processed).
   */
  public function isComplete() {
    $total = $this->source->count(TRUE);
    $imported = $this->importedCount();
    $errors = $this->errorCount();
    return $total <= ($imported + $errors);
  }

  /**
   * Override MigrationBase::beginProcess, to make sure the map/message tables
   * are present.
   *
   * @param int $newStatus
   *  Migration::STATUS_IMPORTING or Migration::STATUS_ROLLING_BACK
   */
  protected function beginProcess($newStatus) {
    parent::beginProcess($newStatus);

    // Make sure the map and message tables are present
    $this->ensureTables();

    // Do some standard setup
    if (isset($this->options['feedback']) && isset($this->options['feedback']['frequency']) &&
        isset($this->options['feedback']['frequency_unit'])) {
      $this->frequency = $this->options['feedback']['frequency'];
      $this->frequency_unit = $this->options['feedback']['frequency_unit'];
    }
    $this->lastfeedback = $this->starttime;

    $this->total_processed = $this->total_successes =
      $this->processed_since_feedback = $this->successes_since_feedback = 0;

    // Call pre-process methods
    if ($this->status == Migration::STATUS_IMPORTING) {
      if (method_exists($this->destination, 'preImport')) {
        $this->destination->preImport($this);
      }
    }
    else {
      if (method_exists($this->destination, 'preRollback')) {
        $this->destination->preRollback($this);
      }
    }
  }

  /**
   * Override MigrationBase::endProcess, to call post hooks
   *
   * @param integer $overallThroughput
   *  Throughput for the entire import. Should be 0 except
   *  at final completion of an import process.
   */
  public function endProcess($overallThroughput = 0) {
    // Call post-process methods
    if ($this->status == Migration::STATUS_IMPORTING) {
      if (method_exists($this->destination, 'postImport')) {
        $this->destination->postImport($this);
      }
    }
    else {
      if (method_exists($this->destination, 'postRollback')) {
        $this->destination->postRollback($this);
      }
    }

    parent::endProcess($overallThroughput);
  }

  /**
   * Perform a rollback operation - remove migrated items from the destination.
   */
  public function rollback() {
    $return = Migration::RESULT_COMPLETED;

    // TODO: Support multi-field keys
    // TODO: Support idlist
    migrate_instrument_start('rollback map query');
    $query = db_select($this->mapTableName, 'map')
             ->fields('map', array('sourceid1', 'destid1'));
    if (isset($this->options['itemlimit'])) {
      $query = $query->range(0, $this->options['itemlimit']);
    }
    $result = $query->execute();
    migrate_instrument_stop('rollback map query');

    // Support bulk rollback (only with single-field keys?)
    if ($this->destination->supportsBulkRollback) {
      // Too many at once can lead to memory issues, so batch 'em up
      $destids = array();
      $sourceids = array();
      $batch_count = 0;
      foreach ($result as $row) {
        $return = $this->checkStatus();
        if ($return != Migration::RESULT_COMPLETED) {
          break;
        }

        $destids[] = $row->destid1;
        $sourceids[] = $row->sourceid1;
        $batch_count++;
        if ($batch_count >= $this->rollbackBatchSize) {
          try {
            if ($this->systemOfRecord == Migration::SOURCE) {
              migrate_instrument_start('destination bulkRollback');
              $this->destination->bulkRollback($this, $destids);
              migrate_instrument_stop('destination bulkRollback');
            }
            // Keep track in case of interruption
            migrate_instrument_start('rollback map/message update');
            db_delete($this->mapTableName)
              ->condition('sourceid1', $sourceids, 'IN')
              ->execute();
            db_delete($this->messageTableName)
              ->condition('sourceid1', $sourceids, 'IN')
              ->execute();
            migrate_instrument_stop('rollback map/message update');
            $this->total_successes += $batch_count;
            $this->successes_since_feedback += $batch_count;
          }
          catch (Exception $e) {
            $output = $this->outputFunction;
            $output($e->getMessage());
            migrate_instrument_stop('bulkRollback');
            migrate_instrument_stop('rollback map/message update');
          }
          $destids = array();
          $sourceids = array();
          $batch_count = 0;
          // Will increment even if there was an exception... But we don't
          // really have a way to know how many really were successfully rolled back
          $this->total_processed += $batch_count;
          $this->processed_since_feedback += $batch_count;
        }
      }
      if ($batch_count > 0) {
        if ($this->systemOfRecord == Migration::SOURCE) {
          migrate_instrument_start('destination bulkRollback');
          $this->destination->bulkRollback($this, $destids);
          migrate_instrument_stop('destination bulkRollback');
          $this->total_processed += $batch_count;
          $this->total_successes += $batch_count;
          $this->processed_since_feedback += $batch_count;
          $this->successes_since_feedback += $batch_count;
        }
        migrate_instrument_start('rollback map/message update');
        db_delete($this->mapTableName)
          ->condition('sourceid1', $sourceids, 'IN')
          ->execute();
        db_delete($this->messageTableName)
          ->condition('sourceid1', $sourceids, 'IN')
          ->execute();
        migrate_instrument_stop('rollback map/message update');
      }
    }
    else {
      foreach ($result as $record) {
        $return = $this->checkStatus();
        if ($return != Migration::RESULT_COMPLETED) {
          break;
        }

        // Rollback one record
        try {
          if ($this->systemOfRecord == Migration::SOURCE) {
            migrate_instrument_start('destination rollback');
            $this->destination->rollback($this, array($record->destid1));
            migrate_instrument_stop('destination rollback');
          }

          migrate_instrument_start('rollback map/message update');
          db_delete($this->mapTableName)
            ->condition('sourceid1', $record->sourceid1)
            ->execute();
          db_delete($this->messageTableName)
            ->condition('sourceid1', $record->sourceid1)
            ->execute();
          migrate_instrument_stop('rollback map/message update');
          $this->total_successes++;
          $this->successes_since_feedback++;
        }
        catch (Exception $e) {
          // TODO: At least count failures
          continue;
        }
        $this->total_processed++;
        $this->processed_since_feedback++;
      }
    }
    // Clear message table
    migrate_instrument_start('rollback map/message update');
    db_delete($this->messageTableName)
      ->execute();
    migrate_instrument_stop('rollback map/message update');

    $this->progressMessage($return);

    // If we're using highwater marks, reset at completion of a full rollback
    // TODO: What about partial rollbacks? Probably little we can do to make
    // that work cleanly...
    if ($this->highwaterField) {
      $this->saveHighwater('');
    }
    return $return;
  }

  /**
   * Perform an import operation - migrate items from source to destination.
   */
  public function import() {
    $return = Migration::RESULT_COMPLETED;

    while ($data_row = $this->source->fetch($this, $this->options)) {
      if ($this->memoryExceeded()) {
        $return = Migration::RESULT_INCOMPLETE;
        break;
      }
      elseif ($this->getStatus() == Migration::STATUS_STOPPING) {
        $return = Migration::RESULT_STOPPED;
        break;
      }

      $this->progressMessage(Migration::RESULT_INCOMPLETE);

      // Track the current source key for use by map/message tables
      $this->currSourceKey = array();
      foreach ($this->sourceKeyMap() as $field_name => $map_name) {
        $this->currSourceKey[$map_name] = $data_row->$field_name;
      }

      // If the source hasn't already filtered out items based on the map and
      // message tables, it's up to us
      if (!$this->source->handlesMapFiltering) {
        // Have to enforce itemlimit here
        if (isset($this->options['itemlimit']) && $this->options['itemlimit'] <= $this->total_processed) {
          break;
        }

        // We still need to check the map, in case we're updating an existing item
        $query = db_select($this->mapTableName, 'map')
                 ->fields('map', array('needs_update', 'destid1'));
        foreach ($this->sourceKeyMap() as $field_name => $map_name) {
          $query = $query->condition("map.$map_name", $data_row->$field_name, '=');
        }
        $row = $query->execute()->fetchObject();
        // Save the dest ID in case of update
        if ($row) {
          $data_row->migrate_map_destid1 = $row->destid1;
        }

        // If we're not using highwater marks, we need to check the map to determine
        // whether this needs migration
        if (!isset($this->highwaterField)) {
          // Needs_update may be NULL (no map table entry), 0 (map entry present
          // but doesn't need update, or 1 (needs update). The 2nd case is the one
          // where we skip this entry
          if ($row && $row->needs_update == 0 && is_string($row->needs_update)) {
            continue;
          }

          // TODO: Check message table (entries here indicate failed items, we don't
          // want to run through them again)

        }
      }

      $values = $this->applyMappings($data_row);

      try {
        migrate_instrument_start('destination import', TRUE);
        $ids = $this->destination->import($this, $values, $data_row);
        migrate_instrument_stop('destination import');
        if ($ids) {
          $this->saveIDMapping($data_row, $ids, $this->needsUpdate);
          $this->successes_since_feedback++;
          $this->total_successes++;
        }
        else {
          // TODO: Failure to save
          $message = t('New object was not saved, no error provided');
          $this->saveMessage($message);
          $outputFunction = $this->outputFunction;
          $outputFunction($message);
        }
      }
      catch (MigrateException $e) {
        $this->saveMessage($e->getMessage(), $e->level);
        $outputFunction = $this->outputFunction;
        $outputFunction($e->getMessage());
      }
      catch (Exception $e) {
        $this->saveMessage($e->getMessage());
        $outputFunction = $this->outputFunction;
        $outputFunction($e->getMessage());
      }
      $this->total_processed++;
      $this->processed_since_feedback++;
      if (isset($this->highwaterField)) {
        $this->saveHighwater($data_row->{$this->highwaterField['name']});
      }

      // Reset row properties.
      unset($values);
      $this->needsUpdate = FALSE;

      // TODO: Temporary. Remove when http://drupal.org/node/375494 is committed.
      // TODO: Should be done in MigrateDestinationEntity (not all destinations
      // are entities)
      if (!empty($this->destination->entityType)) {
        entity_get_controller($this->destination->entityType)->resetCache();
      }
    }

    $this->progressMessage($return);

    return $return;
  }

  ////////////////////////////////////////////////////////////////////
  // Utility methods

  /**
   * Get the number of records successfully imported.
   * @return int
   *  Number of imported records.
   */
  public function importedCount() {
    $this->ensureTables();
    $query = db_select($this->mapTableName);
    $query->addExpression('COUNT(*)', 'count');
    $count = $query->execute()->fetchField();
    return $count;
  }

  /**
   * Get the number of source records which failed to import.
   * TODO: Doesn't yet account for informationals, or multiple errors for
   * a source record.
   *
   * @return int
   *  Number of records errored out.
   */
  public function errorCount() {
    $this->ensureTables();
    $query = db_select($this->messageTableName);
    $query->addExpression('COUNT(*)', 'count');
    $count = $query->execute()->fetchField();
    return $count;
  }

  /**
   * Retrieve a mapping from source key field names to the names used in the
   * map and message tables.
   * @return array
   *  Array keyed by field names in the MigrationSource object, with values of
   *  the form sourceid[1-9] (assumes no PKs with 10 or more fields)
   */
  public function sourceKeyMap() {
    if (!isset($this->sourceKeyMap)) {
      $count = 1;
      $this->sourceKeyMap = array();
      foreach ($this->source->keySchema as $field_name => $field_schema) {
        $this->sourceKeyMap[$field_name] = 'sourceid' . $count++;
      }
    }
    return $this->sourceKeyMap;
  }

  /**
   * Retrieve a mapping from destination key field names to the names used in the
   * map and message tables.
   * @return array
   *  Array keyed by field names in the MigrationDestination object, with values of
   *  the form destid[1-9] (assumes no PKs with 10 or more fields)
   */
  public function destinationKeyMap() {
    if (!isset($this->destinationKeyMap)) {
      $count = 1;
      $this->destinationKeyMap = array();
      foreach ($this->destination->keySchema as $field_name => $field_schema) {
        $this->destinationKeyMap[$field_name] = 'destid' . $count++;
      }
    }
    return $this->destinationKeyMap;
  }

  /**
   * Outputs a progress message, reflecting the current status of a migration process.
   *
   * @param int $result
   *  Status of the process, represented by one of the Migration::RESULT_* constants.
   */
  protected function progressMessage($result) {
    // In the INCOMPLETE (feedback) case, only proceed under the proper conditions
    if ($result == Migration::RESULT_INCOMPLETE) {
      if (isset($this->frequency)) {
        if (($this->frequency_unit == 'seconds' && time()-$this->lastfeedback >= $this->frequency) ||
            ($this->frequency_unit == 'items' && $this->processed_since_feedback >= $this->frequency)) {
          // Fall through
        }
        else {
          return;
        }
      }
      else {
        return;
      }
    }

    $time = microtime(TRUE) - $this->lastfeedback;
    if ($time > 0) {
      $perminute = round(60*$this->processed_since_feedback/$time);
      $time = round($time, 1);
    }
    else {
      $perminute = '?';
    }

    if ($this->status == Migration::STATUS_IMPORTING) {
      switch ($result) {
        case Migration::RESULT_COMPLETED:
          $basetext = "Imported !successes (!failed failed) in !time sec (!perminute/min) - done with '!name'";
          $type = 'completed';
          break;
        case Migration::RESULT_FAILED:
          $basetext = "Imported !successes (!failed failed) in !time sec (!perminute/min) - failure with '!name'";
          $type = 'failed';
          break;
        case Migration::RESULT_INCOMPLETE:
          $basetext = "Imported !successes (!failed failed) in !time sec (!perminute/min) - continuing with '!name'";
          $type = 'ok';
          break;
        case Migration::RESULT_STOPPED:
          $basetext = "Imported !successes (!failed failed) in !time sec (!perminute/min) - stopped '!name'";
          $type = 'warning';
          break;
      }
    }
    else {
      switch ($result) {
        case Migration::RESULT_COMPLETED:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - done with '!name'";
          $type = 'completed';
          break;
        case Migration::RESULT_FAILED:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - failure with '!name'";
          $type = 'failed';
          break;
        case Migration::RESULT_INCOMPLETE:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - continuing with '!name'";
          $type = 'ok';
          break;
        case Migration::RESULT_STOPPED:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - stopped '!name'";
          $type = 'warning';
          break;
      }
    }
    $message = t($basetext,
        array('!numitems' => $this->processed_since_feedback,
              '!successes' => $this->successes_since_feedback,
              '!failed' => $this->processed_since_feedback - $this->successes_since_feedback,
              '!time' => $time,
              '!perminute' => $perminute,
              '!name' => $this->machineName));
    $outputFunction = $this->outputFunction;
    $outputFunction($message, $type);

    // Report on lookup_cache hit rate. Only visible at 'debug' level.
    if ($result != Migration::RESULT_INCOMPLETE && !empty($this->counts['lookup_cache'])) {
      foreach ($this->counts['lookup_cache'] as $name => $tallies) {
        $tallies += array('hit' => 0, 'miss_hit' => 0, 'miss_miss' => 0); // Set defaults to avoid NOTICE.
        $sum = $tallies['hit']+$tallies['miss_hit']+$tallies['miss_miss'];
        $outputFunction(t('Lookup cache: !mn SM=!name !hit hit, !miss_hit miss_hit, !miss_miss miss_miss (!total total).', array(
                '!mn' => $this->machineName,
                '!name' => $name,
                '!hit' => round((100*$tallies['hit'])/$sum) . '%',
                '!miss_hit' => round((100*$tallies['miss_hit'])/$sum) . '%',
                '!miss_miss' => round((100*$tallies['miss_miss'])/$sum) . '%',
                '!total' => $sum
        )), 'debug');
      }
      $this->counts['lookup_cache'] = array();
    }
    if ($result == Migration::RESULT_INCOMPLETE) {
      $this->lastfeedback = time();
      $this->processed_since_feedback = $this->successes_since_feedback = 0;
    }
  }

  /**
   * Fetch the current highwater mark for updated content.
   *
   * @return string
   *  The highwater mark.
   */
  public function highwater() {
    $highwater = db_query("SELECT highwater FROM {migrate_status}
                           WHERE machine_name=:machine_name",
                          array(':machine_name' => $this->machineName))
                ->fetchField();
    return $highwater;
  }

  /**
   * Convenience function to return count of total source records
   *
   * @param boolean $refresh
   *  Pass TRUE to refresh the cached count.
   */
  public function sourceCount($refresh = FALSE) {
    return $this->source->count($refresh);
  }

  /**
   * Get human readable name for a message constant.
   *
   * @return string
   *  Name.
   */
  public function getMessageLevelName($constant) {
    $map = array(
      Migration::MESSAGE_ERROR => t('Error'),
      Migration::MESSAGE_WARNING => t('Warning'),
      Migration::MESSAGE_NOTICE => t('Notice'),
      Migration::MESSAGE_INFORMATIONAL => t('Informational'),
    );
    return $map[$constant];
  }

  /**
   * Standard top-of-loop stuff, common between rollback and import - check
   * for exceptional conditions, and display feedback.
   */
  protected function checkStatus() {
    if ($this->memoryExceeded()) {
      return Migration::RESULT_INCOMPLETE;
    }
    if ($this->getStatus() == Migration::STATUS_STOPPING) {
      return Migration::RESULT_STOPPED;
    }
    $this->progressMessage(Migration::RESULT_INCOMPLETE);
    return Migration::RESULT_COMPLETED;
  }

  /**
   * Apply field mappings to a data row received form the source, returning
   * a populated destination object.
   *
   * @param stdClass $data_row
   */
  protected function applyMappings(stdClass $data_row) {
    // Apply mappings.
    $values = new stdClass;
    foreach ($this->fieldMappings as $mapping) {
      if (isset($mapping->destinationField)) {
        $destination = $mapping->destinationField;
        $source = $mapping->sourceField;
        // Make any direct value assignments
        if ($source && isset($data_row->$source)) {
          $values->$destination = $data_row->$source;
        }
        elseif (!is_null($mapping->defaultValue)) {
          $values->$destination = $mapping->defaultValue;
        }

        // If there's a separator specified for this destination, then it
        // will be populated as an array exploded from the source value
        if (isset($mapping->separator) && isset($values->$destination)) {
          $values->$destination = explode($mapping->separator, $values->$destination);
        }

        // If a source migration is supplied, use the current value for this field
        // to look up a destination ID from the provided migration
        if ($mapping->sourceMigration && isset($values->$destination)) {
          $source_migration = Migration::getInstance($mapping->sourceMigration);
          // Might be an array, after separator processing
          if (is_array($values->$destination)) {
            $source_keys = $values->$destination;
          }
          else {
            $source_keys = array($values->$destination);
          }
          $results = array();
          foreach ($source_keys as $source_key) {
            $destids = $source_migration->lookupDestinationID(array($source_key), $this);
            if ($destids) {
              // Assume that if the destination key is a single value, it
              // should be passed as such
              if (count($destids) == 1) {
                $results[] = $destids['destid1'];
              }
              else {
                $results[] = $destids;
              }
            }
            else {
              $results[] = $mapping->defaultValue;
            }
          }
          if (is_array($values->$destination) || count($results) > 1) {
            $values->$destination = $results;
          }
          else {
            $values->$destination = $results[0];
          }
        }

        // Assign any arguments
        if (isset($values->$destination)) {
          if (isset($mapping->arguments)) {
            if (!is_array($values->$destination)) {
              $values->$destination = array($values->$destination);
            }
            // TODO: Stuffing arguments into the destination field is gross - can
            // we come up with a better way to communicate them to the field
            // handlers?
            $values->{$destination}['arguments'] = array();
            foreach ($mapping->arguments as $argname => $destarg) {
              if (is_array($destarg) && isset($destarg['source_field']) && isset($data_row->$destarg['source_field'])) {
                $values->{$destination}['arguments'][$argname] = $data_row->$destarg['source_field'];
              }
              elseif (is_array($destarg) && isset($destarg['default_value'])) {
                $values->{$destination}['arguments'][$argname] = $destarg['default_value'];
              }
              else {
                $values->{$destination}['arguments'][$argname] = $destarg;
              }
            }
          }
        }
      }
    }

    // If there's a destination ID, the intent is to update an existing object,
    // pass it as the PK
    // TODO: Support multi-column keys
    // TODO: 'id' should be the actual destination field name(s) corresponding to the key
    if (isset($data_row->migrate_map_destid1)) {
      //$values['id'] = $data_row->migrate_map_destid1;
    }
    return $values;
  }
}
