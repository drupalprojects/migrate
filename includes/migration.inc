<?php
// $Id$

/**
 * @file
 * Define the base for the migration controller. This is where most of the smarts
 * of the migrate module resides. Migrations are created by deriving from this
 * class, and in the constructor (after calling parent::__construct()) initializing
 * at a minimum the description, source, and destination properties. The constructor
 * will also usually make several calls to addFieldMapping() and/or
 * addFieldMappingCallback().
 *
 * TODO:
 * Fully support compound keys in sources
 * Fully support compound keys in destinations
 * Support updating (set needs_update=1) - needs http://drupal.org/node/705384
 * Rollback/import hooks (still useful?)
 * Timers
 * number_integer, node ref
 * Better cardinality handling.
 */
abstract class Migration {
  /**
   * The machine name of this Migration object, derived by removing the 'Migration'
   * suffix from the class name. Used to construct default map/message table names,
   * displayed in drush migrate-status, key to migrate_status table...
   *
   * @var string
   */
  public $machineName;

  /**
   * Human-friendly description of the migration, used in some messages.
   *
   * @var string
   */
  public $description;

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
   * Names of tables created for tracking the migration. By default they are
   * constructed from machineName, but a derived Migration class may set them
   * as it wishes.
   *
   * @var string
   */
  public $mapTableName, $messageTableName;

  /**
   * Simple mappings between destination fields (keys) and source fields (values).
   *
   * @var array
   */
  protected $fieldMappings = array();

  /**
   * Default values for simple mappings, when there is no source mapping or the
   * source field is empty - destination fields are the keys, and the
   * default values are the values in the array.
   *
   * @var array
   */
  protected $defaultValues = array();

  /**
   * Separators for simple mappings. Each destination field appearing here will
   * be set up as an array of values exploded from the corresponding source field.
   *
   * @var array
   */
  protected $separators = array();

  /**
   * Argument overrides for simple mappings. Each entry is an array, keyed by
   * a field API array key, with one or both of these entries:
   * 'source_field' - Name of the source field in the incoming row containing the
   *  value to be assigned
   * 'default_value' - A constant value to be assigned in the absence of source_field
   *
   * @var array
   */
  protected $arguments = array();

  /**
   * Callback mappings for destination fields (keys). Each value is an array:
   *  [0] => Callback name (method in the destination class)
   *  [1] => Array of arguments to the particular callback
   *  [2] (optional) => Default value to use if the source field is empty
   *
   * @var array
   */
  protected $fieldMappingsCallback = array();

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
   * List of other Migration classes which should be imported before this one.
   * E.g., a comment migration class would typically have node and user migrations
   * as dependencies.
   *
   * @var array
   */
  public $dependencies = array();

  /**
   * Name of a function for displaying feedback. It must take the message to display
   * as its first argument, and a (string) message type as its second argument
   * (see drush_log()).
   * @var string
   */
  public $outputFunction = 'drush_log';

  /**
   * The fraction of the memory limit at which an operation will be interrupted.
   * Can be overridden by a Migration subclass if one would like to push the
   * envelope.
   *
   * @var float
   */
  protected $memoryThreshold = 0.8;   // 80%

  /**
   * The PHP memory_limit expressed in bytes.
   *
   * @var int
   */
  protected $memoryLimit;

  /**
   * When performing a bulkRollback(), the maximum number of items to pass in
   * a single call. Can be overridden in derived class constructor.
   *
   * @var int
   */
  protected $rollbackBatchSize = 500;

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
   * Codes representing the result of a rollback or import process.
   */
  const RESULT_COMPLETED = 1;   // All records have been processed
  const RESULT_INCOMPLETE = 2;  // The process has interrupted itself (e.g., the
                                // memory limit is approaching)
  const RESULT_STOPPED = 3;     // The process was stopped externally (e.g., via
                                // drush migrate-stop)
  const RESULT_FAILED = 4;      // The process had a fatal error

  /**
   * Codes representing the current status of a migration, and stored in the
   * migrate_status table.
   */
  const STATUS_IDLE = 0;
  const STATUS_IMPORTING = 1;
  const STATUS_ROLLING_BACK = 2;

  /**
   * General initialization of a Migration object.
   */
  public function __construct() {
    // Construct the machine name by stripping Migration from our class name
    $class = get_class($this);
    $this->machineName = substr($class, 0, strlen($class) - strlen('Migration'));

    // Default generated table names - derived class can override
    $this->mapTableName = 'migrate_map_' . drupal_strtolower($this->machineName);
    $this->messageTableName = 'migrate_message_' . drupal_strtolower($this->machineName);

    // Record the memory limit in bytes
    $limit = trim(ini_get('memory_limit'));
    $last = strtolower($limit[strlen($limit)-1]);
    switch ($last) {
      case 'g':
        $limit *= 1024;
      case 'm':
        $limit *= 1024;
      case 'k':
        $limit *= 1024;
        break;
      default:
        throw new Exception(t('Invalid PHP memory_limit !limit',
          array('!limit' => $limit)));
    }
    $this->memoryLimit = $limit;
  }

  ////////////////////////////////////////////////////////////////////
  // Map/message table handling

  /**
   * Lazy creation of map and message tables associated with this migration.
   */
  protected function ensureTables() {
    if (!db_table_exists($this->mapTableName)) {
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
      $index = $this->destinationKeyMap();
      $fields['needs_update'] = array(
        'type' => 'int',
        'size' => 'tiny',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'default' => (int) FALSE,
        'description' => 'Flags existing mapped data to be updated',
      );
      $schema = array(
        'description' => t('Mappings from source key to destination key'),
        'fields' => $fields,
        'primary key' => $pks,
        'unique keys' => array('destkey' => $index),
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
  public function lookupDestinationID(array $source_id) {
    $this->ensureTables();
    $query = db_select($this->mapTableName, 'map');
    foreach ($this->destinationKeyMap() as $key_name) {
      $query->addField('map', $key_name);
    }
    foreach ($this->sourceKeyMap() as $key_name) {
      $query = $query->condition("map.$key_name", array_shift($source_id), '=');
    }
    $result = $query->execute();
    $destination_id = $result->fetchAssoc();
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
    // Construct the source key
    $keys = array();
    foreach ($this->sourceKeyMap() as $field_name => $key_name) {
      $keys[$key_name] = $source_row->$field_name;
    }
    // TODO: Construct field info from destination key info (support multiple fields)
    db_merge($this->mapTableName)
      ->key($keys)
      ->fields(array(
        'destid1' => $dest_ids[0],
        'needs_update' => (int)$needs_update,
      ))
      ->execute();
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
   * Add a simple mapping for a destination field, specifying a source field and/or
   * a default value.
   *
   * @param string $destinationField
   *  Name of the destination field.
   * @param string $sourceField
   *  Name of the source field (optional).
   * @param mixed $defaultValue
   *  A default value, to be applied to the destination if there is no $sourcefield,
   *  or if the source field in the current row is empty.
   * @param string $separator
   *  If present, the source value will be assumed to have multiple values separated
   *  by $separator, and the mapped destination field will be initialized as an array
   *  of the exploded values.
   * @param array $arguments
   *  Additional arguments to pass to the destination import method for this destination
   *  (in particular, assignments for Field API value arrays).
   */
  public function addFieldMapping($destinationField, $sourceField, $defaultValue = NULL,
                                  $separator = NULL, array $arguments = array()) {
    $this->fieldMappings[$destinationField] = $sourceField;
    if (isset($defaultValue)) {
      $this->defaultValues[$destinationField] = $defaultValue;
    }
    if (isset($separator)) {
      $this->separators[$destinationField] = $separator;
    }
    if ($arguments) {
      $this->arguments[$destinationField] = $arguments;
    }
  }

  /**
   * Add a callback mapping for a destination field.
   *
   * @param string $destinationField
   *  Name of the destination field.
   * @param string $callback
   *  Name of the destination class method to call to obtain the value.
   * @param array $arguments
   *  Array of arguments specific to the callback.
   * @param mixed $default
   */
  public function addFieldMappingCallback($destinationField, $callback, array $arguments, $defaultValue = NULL) {
    $this->fieldMappingsCallback[$destinationField] = array($callback, $arguments, $defaultValue);
  }

  /**
   * Check the current status of a migration.
   * @return int
   *  Migration::STATUS_IDLE, Migration::STATUS_IMPORTING, or Migration::STATUS_ROLLING_BACK
   */
  public function getStatus() {
    $status = db_query("SELECT status FROM {migrate_status}
                        WHERE machine_name=:machine_name",
                       array(':machine_name' => $this->machineName))
                ->fetchField();
    if (!isset($status)) {
      $status = Migration::STATUS_IDLE;
    }
    return $status;
  }

  /**
   * Retrieve the last time an import operation completed successfully.
   * @return string
   *  Date/time string, formatted... How? Default DB server format?
   */
  public function getLastImported() {
    $last_imported = db_query("SELECT lastimported FROM {migrate_status}
                        WHERE machine_name=:machine_name",
                       array(':machine_name' => $this->machineName))
                ->fetchField();
    return $last_imported;
  }

  /**
   * Begin a rollback or import process, ensuring only one process can be active
   * at once on a given migration.
   *
   * @param int $newStatus
   *  Migration::STATUS_IMPORTING or Migration::STATUS_ROLLING_BACK
   */
  protected function beginProcess($newStatus) {
    // Try to make the semaphore handling atomic (depends on DB support)
    $transaction = db_transaction();

    // Check to make sure there's no process already running for this migration
    $status = db_query("SELECT status FROM {migrate_status}
                        WHERE machine_name=:machine_name",
                       array(':machine_name' => $this->machineName))
                ->fetchField();
    if ($status != Migration::STATUS_IDLE) {
      throw new Exception(t('There is already an active process on !machine_name',
        array('!machine_name' => $this->machineName)));
    }

    db_merge('migrate_status')
      ->key(array('machine_name' => $this->machineName))
      ->fields(array('status' => $newStatus))
      ->execute();

    // Make sure the map and message tables are present
    $this->ensureTables();
  }

  /**
   * End a rollback or import process, releasing the semaphore.
   *
   * @param boolean $setTime
   *  If TRUE, record the current time as lastimported (should be set to TRUE only
   *  at final completion of an import process).
   */
  public function endProcess($setTime = FALSE) {
    $fields = array('status' => Migration::STATUS_IDLE);
    if ($setTime) {
      $fields['lastimported'] = date('Y-m-d H:i:s');
    }
    db_merge('migrate_status')
      ->key(array('machine_name' => $this->machineName))
      ->fields($fields)
      ->execute();
  }

  /**
   * Perform a rollback operation - remove migrated items from the destination.
   *
   * @param array $options
   *  All options are, of course, optional.
   *  itemlimit - If present, process no more than this many items.
   *  idlist - If present, process only items with source keys in this
   *    comma-separated list. NOTE: Only supported for sources with single-column
   *    keys.
   *  feedback - If present, must be an array with 'frequency' and 'frequency_unit'
   *    keys. frequency_unit may be 'items' or 'seconds'. A status message will
   *    be generated periodically - after 'frequency' items have been processed, or
   *    after 'frequency' seconds have passed.
   */
  public function rollback(array $options = array()) {
    $this->beginProcess(Migration::STATUS_ROLLING_BACK);

    // TODO: Validate dependencies (migrations dependent on us have no imported rows)

    // TODO: Shared with import - move to beginProcess and track this stuff as properties?
    $starttime = microtime(TRUE);
    $return = Migration::RESULT_COMPLETED;
    if (isset($options['feedback']) && isset($options['feedback']['frequency']) &&
        isset($options['feedback']['frequency_unit'])) {
      $frequency = isset($options['feedback']['frequency']);
      $frequency_unit = isset($options['feedback']['frequency_unit']);
      $lastfeedback = time();
    }

    // TODO: Support multi-field keys
    // TODO: Support idlist and itemlimit
    $result = db_select($this->mapTableName, 'map')
             ->fields('map', array('sourceid1', 'destid1'))
             ->execute();
    // Support bulk rollback (only with single-field keys?)
    if ($this->destination->supportsBulkRollback) {
      // Too many at once can lead to memory issues, so batch 'em up
      $destids = array();
      $sourceids = array();
      $count = $total = 0;
      foreach ($result as $row) {
        if ($this->memoryExceeded()) {
          $return = Migration::RESULT_INCOMPLETE;
          break;
        }
        $destids[] = $row->destid1;
        $sourceids[] = $row->sourceid1;
        $count++;
        $total++;
        if ($count >= $this->rollbackBatchSize) {
          $this->destination->bulkRollback($this, $destids);
          // Keep track in case of interruption
          db_delete($this->mapTableName)
            ->condition('sourceid1', $sourceids, 'IN')
            ->execute();
          db_delete($this->messageTableName)
            ->condition('sourceid1', $sourceids, 'IN')
            ->execute();
          $destids = array();
          $sourceids = array();
          $count = 0;
        }
      }
      if ($count > 0) {
        $this->destination->bulkRollback($this, $destids);
      }
      db_truncate($this->mapTableName)->execute();
      db_truncate($this->messageTableName)->execute();
    }
    else {
      $total = 0;
      // TODO: Can this chunk (shared with import) be factored out?
      foreach ($result as $record) {
        if ($this->memoryExceeded()) {
          $return = Migration::RESULT_INCOMPLETE;
          break;
        }
        if (isset($frequency)) {
          if (($frequency_unit == 'seconds' && time()-$lastfeedback >= $frequency) ||
              ($frequency_unit == 'items' && $count >= $frequency)) {
            $this->progressMessage($starttime, $count, FALSE, Migration::RESULT_INCOMPLETE);
            $lastfeedback = time();
            $total = 0;
          }
        }
        if ($this->getStatus() == Migration::STATUS_IDLE) {
          $return = Migration::RESULT_STOPPED;
          break;
        }

        // Rollback one record
        $this->destination->rollback($this, array($record->destid1));

        db_delete($this->mapTableName)
          ->condition('sourceid1', $record->sourceid1)
          ->execute();
        db_delete($this->messageTableName)
          ->condition('sourceid1', $record->sourceid1)
          ->execute();
        $total++;
      }
    }
    $this->progressMessage($starttime, $total, FALSE, $return);
    $this->endProcess();
  }

  /**
   * Perform an import operation - migrate items from source to destination.
   *
   * @param array $options
   *  All options are, of course, optional.
   *  itemlimit - If present, process no more than this many items.
   *  idlist - If present, process only items with source keys in this
   *    comma-separated list. NOTE: Only supported for sources with single-column
   *    keys.
   *  feedback - If present, must be an array with 'frequency' and 'frequency_unit'
   *    keys. frequency_unit may be 'items' or 'seconds'. A status message will
   *    be generated periodically - after 'frequency' items have been processed, or
   *    after 'frequency' seconds have passed.
   */
  public function import(array $options = array()) {
    $this->beginProcess(Migration::STATUS_IMPORTING);
    try {
      // TODO: Validate dependencies (migrations we depend on have no unimported rows)
      $starttime = microtime(TRUE);
      $return = Migration::RESULT_COMPLETED;
      if (isset($options['feedback']) && isset($options['feedback']['frequency']) &&
          isset($options['feedback']['frequency_unit'])) {
        $frequency = $options['feedback']['frequency'];
        $frequency_unit = $options['feedback']['frequency_unit'];
        $lastfeedback = time();
      }
      $count = 0;
      while ($data_row = $this->source->fetch($this, $options)) {
        if ($this->memoryExceeded()) {
          $return = Migration::RESULT_INCOMPLETE;
          break;
        }
        if (isset($frequency)) {
          if (($frequency_unit == 'seconds' && time()-$lastfeedback >= $frequency) ||
              ($frequency_unit == 'items' && $count >= $frequency)) {
            $this->progressMessage($starttime, $count, TRUE, Migration::RESULT_INCOMPLETE);
            $lastfeedback = time();
            $count = 0;
          }
        }
        if ($this->getStatus() == Migration::STATUS_IDLE) {
          $return = Migration::RESULT_STOPPED;
          break;
        }

        // Track the current source key for use by map/message tables
        $this->currSourceKey = array();
        foreach ($this->sourceKeyMap() as $field_name => $map_name) {
          $this->currSourceKey[$map_name] = $data_row->$field_name;
        }

        // If the source hasn't already filtered out items based on the map and
        // message tables, it's up to us
        if (!$this->source->handlesMapFiltering) {
          // So, if this source ID is present in the map table or the message table,
          // skip it - unless its map table entry has needs_update=1
          $query = db_select($this->mapTableName, 'map');
          $query->addField('map', 'needs_update');
          foreach ($this->sourceKeyMap() as $field_name => $map_name) {
            $query = $query->condition("map.$map_name", $data_row->$field_name, '=');
          }
          $result = $query->execute();
          $needs_update = $result->fetchField();
          // Needs_update may be NULL (no map table entry), 0 (map entry present
          // but doesn't need update, or 1 (needs update). The 2nd case is the one
          // where we skip this entry
          if ($needs_update == 0 && is_string($needs_update)) {
            continue;
          }

          // TODO: Check message table (entries here indicate failed items, we don't
          // want to run through them again)
        }

        // Apply simple mappings.
        $values = new stdClass;
        $arguments = array();
        foreach ($this->fieldMappings as $destination => $source) {
          // Make any direct value assignments
          if (isset($data_row->$source)) {
            $values->$destination = $data_row->$source;
          }
          elseif (isset($this->defaultValues[$destination])) {
            $values->$destination = $this->defaultValues[$destination];
          }

          // Assign any arguments
          $arguments[$destination] = array();
          if (isset($this->arguments[$destination])) {
            foreach ($this->arguments[$destination] as $argname => $destarg) {
              if (isset($destarg['source_field']) && isset($data_row->$destarg['source_field'])) {
                $arguments[$destination][$argname] = $data_row->$destarg['source_field'];
              }
              elseif (isset($destarg['default_value'])) {
                $arguments[$destination][$argname] = $destarg['default_value'];
              }
            }
          }

          // If there's a separator specified for this destination, then it
          // will be populated as an array exploded from the source value
          if (isset($this->separators[$destination]) && isset($values->$destination)) {
            $separator = $this->separators[$destination];
            $values->$destination = explode($separator, $values->$destination);
          }
        }

        // These mappings run through a prepare function.
        foreach ($this->fieldMappingsCallback as $destination => $args) {
          list($method, $arguments, $default) = $args;
          $result = $this->destination->$method($data_row, $arguments, $destination);
          $values->$destination = is_null($result) ? $default : $result;
        }


        // If there's a destination ID, the intent is to update an existing object,
        // pass it as the PK
        // TODO: Support multi-column keys
        // TODO: 'id' should be the actual destination field name(s) corresponding to the key
        if (isset($data_row->migrate_map_destid1)) {
          $values['id'] = $data_row->migrate_map_destid1;
        }

        // TODO: Call prepare and complete hooks here, so destinations don't
        // need to worry about it (and we don't have to pass $data_row to them)?
        $ids = $this->destination->import($this, $values, $data_row, $arguments);
        if ($ids) {
          $this->saveIDMapping($data_row, $ids);
          $count++;
        }
        else {
          // TODO: Failure to save
          //$this->saveMesage(...)
          $return = Migration::RESULT_FAILED;
        }
        unset($values);
      }
      // TODO: Factor out to endProcess?
      $time = microtime(TRUE) - $starttime;
      if ($time > 0) {
        $perminute = round(60*$count/$time);
        $time = round($time, 1);
      }
      else {
        $perminute = '?';
      }
      $this->progressMessage($starttime, $count, TRUE, $return);
      $this->endProcess($return == Migration::RESULT_COMPLETED);
    }
    catch (Exception $exception) {
      // If something bad happened, make sure we clear the semaphore
      $this->endProcess(Migration::RESULT_FAILED);
      throw $exception;
    }
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
   * @param float $starttime
   *  UNIX timestamp with microseconds, the time of the beginning of the process or the
   *  last progress message.
   * @param int $numitems
   *  Number of items processed since $starttime.
   * @param boolean $import
   *  TRUE for an import process, FALSE for a rollback process.
   * @param int $result
   *  Status of the process, represented by one of the Migration::RESULT_* constants.
   */
  protected function progressMessage($starttime, $numitems, $import, $result) {
    $time = microtime(TRUE) - $starttime;
    if ($time > 0) {
      $perminute = round(60*$numitems/$time);
      $time = round($time, 1);
    }
    else {
      $perminute = '?';
    }

    if ($import) {
      switch ($result) {
        case Migration::RESULT_COMPLETED:
          $basetext = "Imported !numitems in !time sec (!perminute/min) - done with '!description'";
          $type = 'completed';
          break;
        case Migration::RESULT_FAILED:
          $basetext = "Imported !numitems in !time sec (!perminute/min) - failure with '!description'";
          $type = 'failed';
          break;
        case Migration::RESULT_INCOMPLETE:
          $basetext = "Imported !numitems in !time sec (!perminute/min) - continuing with '!description'";
          $type = 'ok';
          break;
        case Migration::RESULT_STOPPED:
          $basetext = "Imported !numitems in !time sec (!perminute/min) - stopped '!description'";
          $type = 'warning';
          break;
      }
    }
    else {
      switch ($result) {
        case Migration::RESULT_COMPLETED:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - done with '!description'";
          $type = 'completed';
          break;
        case Migration::RESULT_FAILED:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - failure with '!description'";
          $type = 'failed';
          break;
        case Migration::RESULT_INCOMPLETE:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - continuing with '!description'";
          $type = 'ok';
          break;
        case Migration::RESULT_STOPPED:
          $basetext = "Rolled back !numitems in !time sec (!perminute/min) - stopped '!description'";
          $type = 'warning';
          break;
      }
    }
    $message = t($basetext,
        array('!numitems' => $numitems, '!time' => $time, '!perminute' => $perminute,
              '!description' => $this->description));
    $outputFunction = $this->outputFunction;
    $outputFunction($message, $type);
  }

  /**
   * Test whether we've exceeded the desired memory threshold. If so, output a message.
   * @return boolean
   *  TRUE if the threshold is exceeded, FALSE if not.
   *
   * TODO: Throw a MigrationException?
   */
  protected function memoryExceeded() {
    $usage = memory_get_usage();
    $pct_memory = $usage/$this->memoryLimit;
    if ($pct_memory > $this->memoryThreshold) {
      $outputFunction = $this->outputFunction;
      $outputFunction(t('Memory usage is !usage (!pct% of limit !limit), starting new batch',
        array('!pct' => round($pct_memory*100), '!usage' => memory_get_usage(), '!limit' => $this->memoryLimit)),
        'warning');
      return TRUE;
    }
    else {
      return FALSE;
    }
  }
}
