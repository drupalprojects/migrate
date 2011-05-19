<?php

/**
 * @file
 * Support for files as destinations.
 */

/**
 * Destination class implementing migration into the files table.
 */
class MigrateDestinationFile extends MigrateDestinationEntity {
  /**
   * Whether to copy a file from the provided URI into the Drupal installation.
   * If FALSE, the URI is presumed to be local to the Drupal install. If TRUE,
   * the file will be copied according to the copyScheme value.
   *
   * @var boolean
   */
  protected $copyFile = FALSE;

  /**
   * When copying files, the destination scheme/directory. Defaults to 'public://'.
   *
   * @var string
   */
  protected $copyDestination = 'public://';

  static public function getKeySchema() {
    return array(
      'fid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'file_managed ID',
      ),
    );
  }

  /**
   * Return an options array for file destinations.
   *
   * @param boolean $copy_file
   *  TRUE to have the file copied from the provided URI to Drupal. Defaults to FALSE.
   * @param string $copy_destination
   *  If $copy_file is TRUE, the scheme/directory to use as the destination for the copied file.
   *  Defaults to 'public://'.
   * @param string $language
   *  Default language for files created via this destination class.
   * @param string $text_format
   *  Default text format for files created via this destination class.
   */
  static public function options($copy_file, $copy_destination, $language, $text_format) {
    return compact('copy_file', 'copy_destination', 'language', 'text_format');
  }

  /**
   * Basic initialization
   *
   * @param array $options
   *  Options applied to files.
   */
  public function __construct(array $options = array()) {
    if (isset($options['copy_file'])) {
      $this->copyFile = $options['copy_file'];
    }
    if (isset($options['copy_destination'])) {
      $this->copyDestination = $options['copy_destination'];
    }
    parent::__construct('file', 'file', $options);
  }

  /**
   * Returns a list of fields available to be mapped for the entity type (bundle)
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $fields = array();
    // First the core properties
    $fields['fid'] = t('File: Existing file ID');
    $fields['uid'] = t('File: Uid of user associated with file');
    $fields['filename'] = t('File: Name of the file with no path components');
    $fields['uri'] = t('URI of the file');
    $fields['filemime'] = t('File: The file\'s MIME type');
    $fields['status'] = t('File: A bitmapped field indicating the status of the file');
    $fields['timestamp'] = t('File: UNIX timestamp for the date the file was added');

    // Then add in anything provided by handlers
    $fields += migrate_handler_invoke_all('Entity', 'fields', $this->entityType, $this->bundle);
    $fields += migrate_handler_invoke_all('File', 'fields');

    return $fields;
  }

  /**
   * Delete a file entry.
   *
   * @param array $fid
   *  Fid to delete, arrayed.
   */
  public function rollback(array $fid) {
    migrate_instrument_start('file_delete');
    $file = file_load(reset($fid));
    if ($file) {
      // TODO: Error checking
      file_delete($file);
    }
    migrate_instrument_stop('file_delete');
  }

  /**
   * Import a single file record.
   *
   * @param $file
   *  File object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields (fid only in this case) of the file that was saved if
   *  successful. FALSE on failure.
   */
  public function import(stdClass $file, stdClass $row) {
    // Updating previously-migrated content?
    $migration = Migration::currentMigration();
    if (isset($row->migrate_map_destid1)) {
      if (isset($file->fid)) {
        if ($file->fid != $row->migrate_map_destid1) {
          throw new MigrateException(t("Incoming fid !fid and map destination fid !destid1 don't match",
            array('!fid' => $file->fid, '!destid1' => $row->migrate_map_destid1)));
        }
      }
      else {
        $file->fid = $row->migrate_map_destid1;
      }
    }
    if ($migration->getSystemOfRecord() == Migration::DESTINATION) {
      if (!isset($file->fid)) {
        throw new MigrateException(t('System-of-record is DESTINATION, but no destination fid provided'));
      }
      $old_file = file_load($file->fid);
    }

    if ($this->copyFile) {
      $path = trim(parse_url($file->uri, PHP_URL_PATH), '/');
      $destination = $this->copyDestination . $path;
      $dirname = drupal_dirname($destination);
      if (file_prepare_directory($dirname, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)) {
        // We'd like to use file_unmanaged_copy, but it calls file_exists, which
        // won't work for remote URLs
        migrate_instrument_start('MigrateDestinationFile copy');
        $result = @copy(str_replace(' ', '%20', $file->uri), $destination);
        migrate_instrument_stop('MigrateDestinationFile copy');
        if ($result) {
          $file->uri = $destination;
        }
        else {
          throw new MigrateException(t('Could not copy file !uri', array('!uri' => $file->uri)),
            MigrationBase::MESSAGE_ERROR);
        }
      }
      else {
        throw new MigrateException(t('Could not create directory !dirname',
            array('!dirname' => $dirname)),
          MigrationBase::MESSAGE_ERROR);
      }
    }

    // Default filename to the basename of the (final) URI
    if (!isset($file->filename) || !$file->filename) {
      $file->filename = basename($file->uri);
    }

    // Detect the mime type if not provided
    if (!isset($file->filemime) || !$file->filemime) {
      $file->filemime = file_get_mimetype($file->filename);
    }
    // Invoke migration prepare handlers
    $this->prepare($file, $row);

    if (isset($file->fid)) {
      $updating = TRUE;
    }
    else {
      $updating = FALSE;
    }
    migrate_instrument_start('file_save');
    $file = file_save($file);
    migrate_instrument_stop('file_save');
    $this->complete($file, $row);
    if (isset($file->fid)) {
      $return = array($file->fid);
      if ($updating) {
        $this->numUpdated++;
      }
      else {
        $this->numCreated++;
      }
    }
    else {
      $return = FALSE;
    }
    return $return;
  }
}
