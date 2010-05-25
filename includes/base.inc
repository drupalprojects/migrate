<?php

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
 * The base class for all objects representing distinct steps in a migration
 * process. Most commonly these will be Migration objects which actually import
 * data from a source into a Drupal destination, but by deriving classes directly
 * from MigrationBase one can have other sorts of tasks (e.g., enabling/disabling
 * of modules) occur during the migration process.
 */
abstract class MigrationBase {
  /**
   * Singleton-lite implementation - callers should call static getInstance() so
   * only one copy of a given Migration class is instantiated.
   *
   * @var array
   *  List of migration objects keys by class name.
   */
  protected static $migrations = array();

  /**
   * The machine name of this Migration object, derived by removing the 'Migration'
   * suffix from the class name. Used to construct default map/message table names,
   * displayed in drush migrate-status, key to migrate_status table...
   *
   * @var string
   */
  public $machineName;

  /**
   * Detailed information describing the migration.
   *
   * @var string
   */
  public $description;

  /**
   * Save options passed to current operation
   * @var array
   */
  public $options;

  /**
   * Indicates that we are processing a rollback or import - used to avoid
   * excess writes in endProcess()
   *
   * @var boolean
   */
  protected $processing = FALSE;

  /**
   * Are we importing, rolling back, or doing nothing?
   *
   * @var unknown_type
   */
  public $status = Migration::STATUS_IDLE;

  /**
   * When the current operation started.
   * @var int
   */
  protected $starttime;

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
  public $outputFunction;

  /**
   * The fraction of the memory limit at which an operation will be interrupted.
   * Can be overridden by a Migration subclass if one would like to push the
   * envelope. Defaults to 80%.
   *
   * @var float
   */
  protected $memoryThreshold = 0.8;

  /**
   * The PHP memory_limit expressed in bytes.
   *
   * @var int
   */
  protected $memoryLimit;

  /**
   * MigrateTeamMember objects representing people involved with this
   * migration.
   *
   * @var array
   */
  public $team = array();

  /**
   * If provided, an URL for an issue tracking system containing :id: where
   * the issue number will go (e.g., 'http://example.com/project/ticket/:id:').
   *
   * @var string
   */
  public $issuePattern;

  /**
   * If we set an error handler (during import), remember the previous one so
   * it can be restored.
   *
   * @var callback
   */
  protected $previousErrorHandler = NULL;

  /**
   * Codes representing the result of a rollback or import process.
   */
  const RESULT_COMPLETED = 1;   // All records have been processed
  const RESULT_INCOMPLETE = 2;  // The process has interrupted itself (e.g., the
                                // memory limit is approaching)
  const RESULT_STOPPED = 3;     // The process was stopped externally (e.g., via
                                // drush migrate-stop)
  const RESULT_FAILED = 4;      // The process had a fatal error
  const RESULT_SKIPPED = 5;     // Dependencies are unfulfilled - skip the process

  /**
   * Codes representing the current status of a migration, and stored in the
   * migrate_status table.
   */
  const STATUS_IDLE = 0;
  const STATUS_IMPORTING = 1;
  const STATUS_ROLLING_BACK = 2;
  const STATUS_STOPPING = 3;

  /**
   * General initialization of a MigrationBase object.
   */
  public function __construct() {
    // Construct the machine name by stripping Migration from our class name
    $class = get_class($this);
    $this->machineName = substr($class, 0, strlen($class) - strlen('Migration'));

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

    // Default the outputFunction based on context
    if (function_exists('drush_get_option')) {
      $this->outputFunction = 'drush_log';
    }
    else {
      $this->outputFunction = 'drupal_set_message';
    }

    // Make sure we clear our semaphores in case of abrupt exit
    register_shutdown_function(array($this, 'endProcess'));
  }

  /**
   * Return the instance of the given class.
   */
  static public function getInstance($class_name) {
    // Otherwise might miss cache hit on case difference
    $class_name = strtolower($class_name);
    $migrations = &self::$migrations;
    if (!isset($migrations[$class_name])) {
      $migrations[$class_name] = new $class_name;
    }
    return $migrations[$class_name];
  }

  /**
   * There's no relevant message table, so simply output any messages we receive
   *
   * @param string $message
   *  The message to record.
   * @param int $level
   *  Optional message severity (defaults to MESSAGE_ERROR).
   */
  public function saveMessage($message, $level = Migration::MESSAGE_ERROR) {
    $output = $this->outputFunction;
    $output($message, $level);
  }

  /**
   * Custom PHP error handler.
   *
   * @param $error_level
   *   The level of the error raised.
   * @param $message
   *   The error message.
   * @param $filename
   *   The filename that the error was raised in.
   * @param $line
   *   The line number the error was raised at.
   * @param $context
   *   An array that points to the active symbol table at the point the error occurred.
   */
  function errorHandler($error_level, $message, $filename, $line, $context) {
    if ($error_level & error_reporting()) {
      $message .= "\n" . t('File !file, line !line',
        array('!line' => $line, '!file' => $filename));
      // Record notices and continue
      if ($error_level == E_NOTICE || $error_level == E_USER_NOTICE) {
        $this->saveMessage($message . "(file: $filename, line $line)", Migration::MESSAGE_INFORMATIONAL);
      }
      // Simply ignore strict and deprecated errors
      // Note DEPRECATED constants introduced in PHP 5.3
      elseif (!($error_level == E_STRICT || $error_level == 8192 ||
                $error_level == 16384)) {
        throw new MigrateException($message, Migration::MESSAGE_ERROR);
      }
    }
  }

  public function loggingCallback($type, $message, $variables = array(),
          $severity = WATCHDOG_NOTICE, $link = NULL) {
    if ($severity == WATCHDOG_NOTICE || $severity == WATCHDOG_INFO ||
        $severity == WATCHDOG_DEBUG) {
      $this->saveMessage($message, Migration::MESSAGE_INFORMATIONAL);
    }
    else {
      throw new MigrateException($message, Migration::MESSAGE_ERROR);
    }
  }

  /**
   * Check the current status of a migration.
   * @return int
   *  Migration::STATUS_IDLE, Migration::STATUS_IMPORTING, Migration::STATUS_ROLLING_BACK,
   *  or Migration::STATUS_STOPPING
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
   * Reports whether this migration process is complete. For a Migration, for
   * example, this would be whether all available source rows have been processed.
   * Other MigrationBase classes will need to return TRUE/FALSE appropriately.
   */
  abstract public function isComplete();

  /**
   * Reports whether all dependencies have completed migration
   * TODO: Support rollback (reverse dependencies)
   */
  public function dependenciesComplete() {
    foreach ($this->dependencies as $dependency) {
      $migration = Migration::getInstance($dependency);
      if (!$migration->isComplete()) {
        return FALSE;
      }
    }
    return TRUE;
  }

  /**
   * Begin a process, ensuring only one process can be active
   * at once on a given migration.
   * TODO: Add hooks/handlers to be executed once per overall process,
   * and once per individual migration
   *
   * @param int $newStatus
   *  Migration::STATUS_IMPORTING or Migration::STATUS_ROLLING_BACK
   */
  protected function beginProcess($newStatus) {
    // Try to make the semaphore handling atomic (depends on DB support)
    $transaction = db_transaction();

    $this->starttime = microtime(TRUE);

    // Check to make sure there's no process already running for this migration
    $status = db_query("SELECT status FROM {migrate_status}
                        WHERE machine_name=:machine_name",
                       array(':machine_name' => $this->machineName))
                ->fetchField();
    if ($status != Migration::STATUS_IDLE) {
      throw new MigrateException(t('There is already an active process on !machine_name',
        array('!machine_name' => $this->machineName)));
    }

    $this->processing = TRUE;
    $this->status = $newStatus;
    db_merge('migrate_status')
      ->key(array('machine_name' => $this->machineName))
      ->fields(array('status' => $newStatus))
      ->execute();

    // Set an error handler for imports
    if ($newStatus == Migration::STATUS_IMPORTING) {
      $this->previousErrorHandler = set_error_handler(array($this, 'errorHandler'));
    }

    // And capture anything the DB tries to log to the watchdog
    Database::setLoggingCallback(array($this, 'loggingCallback'), WATCHDOG_NOTICE, WATCHDOG_ERROR);
  }

  /**
   * End a rollback or import process, releasing the semaphore.
   * TODO: Add hooks/handlers to be executed once per overall process,
   * and once per individual migration
   *
   * @param integer $overallThroughput
   *  Throughput for the entire import. Should be 0 except
   *  at final completion of an import process.
   */
  public function endProcess($overallThroughput = 0) {
    if ($this->previousErrorHandler) {
      set_error_handler($this->previousErrorHandler);
      $this->previousErrorHandler = NULL;
    }
    if ($this->processing) {
      $this->status = Migration::STATUS_IDLE;
      $fields = array('status' => Migration::STATUS_IDLE);
      if ($overallThroughput) {
        $fields['lastimported'] = gmdate('Y-m-d H:i:s');
        $fields['lastthroughput'] = $overallThroughput;
      }
      db_merge('migrate_status')
        ->key(array('machine_name' => $this->machineName))
        ->fields($fields)
        ->execute();
      $this->processing = FALSE;
    }
  }

  /**
   * Signal that any current import or rollback process should end itself at
   * the earliest opportunity
   */
  public function stopProcess() {
    // Do not change the status of an idle migration
    db_update('migrate_status')
      ->fields(array('status' => Migration::STATUS_STOPPING))
      ->condition('machine_name', $this->machineName)
      ->condition('status', Migration::STATUS_IDLE, '<>')
      ->execute();
  }

  /**
   * Reset the status of the migration to IDLE (to be used when the status
   * gets stuck, e.g. if a process core-dumped)
   */
  public function resetStatus() {
    // Do not change the status of an already-idle migration
    db_update('migrate_status')
      ->fields(array('status' => Migration::STATUS_IDLE))
      ->condition('machine_name', $this->machineName)
      ->condition('status', Migration::STATUS_IDLE, '<>')
      ->execute();
  }

  /**
   * Perform an operation during the rollback phase.
   *
   * @param array $options
   *  List of options provided (usually from a drush command). Specific to
   *  the derived class.
   */
  public function processRollback(array $options = array()) {
    $this->options = $options;
    $return = Migration::RESULT_COMPLETED;
    // TODO: Validate dependencies (migrations depending on us have no imported rows)
/*    if (!isset($options['force'])) {
      if (!$this->dependenciesComplete()) {
        return Migration::RESULT_SKIPPED;
      }
    }*/
    $this->beginProcess(Migration::STATUS_ROLLING_BACK);
    try {
      $return = $this->rollback();
    }
    catch (Exception $exception) {
      // If something bad happened, make sure we clear the semaphore
      $this->endProcess();
      throw $exception;
    }
    $this->endProcess();
    return $return;
  }

  /**
   * Perform an operation during the import phase
   *
   * @param array $options
   *  List of options provided (usually from a drush command). Specific to
   *  the derived class.
   */
  public function processImport(array $options = array()) {
    $this->options = $options;
    if (!isset($options['force'])) {
      if (!$this->dependenciesComplete()) {
        return Migration::RESULT_SKIPPED;
      }
    }
    $this->beginProcess(Migration::STATUS_IMPORTING);
    try {
      $return = $this->import();
    }
    catch (Exception $exception) {
      // If something bad happened, make sure we clear the semaphore
      $this->endProcess();
      throw $exception;
    }

    if ($return == Migration::RESULT_COMPLETED && isset($this->successes)) {
      $overallThroughput = round(60*$this->successes / (microtime(TRUE) - $this->starttime));
    }
    else {
      $overallThroughput = FALSE;
    }
    $this->endProcess($overallThroughput);
    return $return;
  }

  /**
   * Null implementations - if a migration process only needs to do work
   * in one phase (import or rollback), it need not define the other - it will
   * simply report success
   */
  public function rollback() {
    return Migration::RESULT_COMPLETED;
  }
  public function import() {
    return Migration::RESULT_COMPLETED;
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
      $outputFunction(t('Memory usage is !usage (!pct% of limit !limit), resetting statics',
        array('!pct' => round($pct_memory*100), '!usage' => format_size(memory_get_usage()), '!limit' => format_size($this->memoryLimit))),
          'warning');
      drupal_static_reset();
      $usage = memory_get_usage();
      $pct_memory = $usage/$this->memoryLimit;
      // Use a lower threshold - we don't want to be in a situation where we keep
      // coming back here and trimming a tiny amount
      if ($pct_memory > (.85 * $this->memoryThreshold)) {
        $outputFunction(t('Memory usage is now !usage (!pct% of limit !limit), not enough reclaimed, starting new batch',
          array('!pct' => round($pct_memory*100), '!usage' => format_size(memory_get_usage()), '!limit' => format_size($this->memoryLimit))),
            'warning');
        return TRUE;
      }
      else {
        $outputFunction(t('Memory usage is now !usage (!pct% of limit !limit), reclaimed enough, continuing',
          array('!pct' => round($pct_memory*100), '!usage' => format_size(memory_get_usage()), '!limit' => format_size($this->memoryLimit))),
            'warning');
        return FALSE;
      }
    }
    else {
      return FALSE;
    }
  }

}
