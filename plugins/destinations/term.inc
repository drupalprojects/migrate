<?php
// $Id$

/**
 * @file
 * Support for taxonomy term destinations.
 */

// TODO:
// Make sure this works with updates, explicit destination keys
// taxonomy_term_save() is doing a cache_clear_all and an automatic insertion for parent.

/**
 * Destination class implementing migration into terms.
 */
class MigrateDestinationTerm extends MigrateDestinationEntity {
  /**
   * Basic initialization
   */
  public function __construct($bundle, $language = NULL, $text_format = NULL) {
    // PK definition, for use in constructing map/message tables
    $destination_key = array(
      'tid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'ID of destination term',
      ),
    );
    parent::__construct($destination_key, 'taxonomy_term', $bundle, $language, $text_format);
  }

  /**
   * Returns a list of fields available to be mapped for this vocabulary (bundle)
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $fields = array();
    // First the core (taxonomy_term_data table) properties
    $fields['tid'] = t('Term: Existing term ID');
    $fields['name'] = t('Term: Name');
    $fields['description'] = t('Term: Description');
    $fields['format'] = t('Term: Format');
    $fields['weight'] = t('Term: Weight');
    // TODO: Only if vocab has hierarchy
    $fields['parent_name'] = t('Term: Parent (by name)');
    $fields['parent'] = t('Term: Parent (by Drupal term ID)');

    // Then add any fields attached to this vocabulary (bundle)
    $field_instance_info = field_info_instances($this->entityType, $this->bundle);
    foreach ($field_instance_info as $machine_name => $instance) {
      $field_info = field_info_field($machine_name);

      $fields[$machine_name] = t('Term:') . ' ' . $instance['label'] .
        ' (' . $field_info['type'] . ')';
    }

    // Then add in anything provided by handlers
    $fields += migrate_handler_invoke_all('Term', 'fields');

    return $fields;
  }

  /**
   * Delete a migrated term
   *
   * @param $migration
   *  Controlling migration object. Unused here.
   * @param $ids
   *  Array of fields representing the key (in this case, just tid).
   */
  public function rollback(Migration $migration, array $key) {
    $tid = $key[0];

    /*
     * This load() happens soon delete() anyway. We load here in order to
     * avoid notices when term has already been deleted. That is easily possible
     * considering how deleting a term parent also deletes children in same call.
     */
    migrate_instrument_start('taxonomy_term_load');
    if (taxonomy_term_load($tid)) {
      migrate_instrument_stop('taxonomy_term_load');
      migrate_instrument_start('taxonomy_term_delete');
      $result = (bool) taxonomy_term_delete($tid);
      migrate_instrument_stop('taxonomy_term_delete');
    }
    else {
      migrate_instrument_stop('taxonomy_term_load');
      // If it didn't exist, consider this a success
      $result = TRUE;
    }
    return $result;
  }

  /**
   * Import a single term.
   *
   * @param $migration
   *  Controlling migration object. Passed through to prepare/complete handlers.
   * @param $term
   *  Term object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields (tid only in this case) of the term that was saved if
   *  successful. FALSE on failure.
   */
  public function import(Migration $migration, stdClass $term, stdClass $row) {
    // Handle update case
    if (isset($row->migrate_map_destid1)) {
      $term->tid = $row->migrate_map_destid1;
    }

    // Trying to update an existing term
    if (isset($term->tid)) {
      $existing_term = taxonomy_term_load($term->tid);
      if ($existing_term) {
        // Incoming data overrides existing data, so only copy non-existent fields
        foreach ($existing_term as $field => $value) {
          if (!isset($term->$field)) {
            $term->$field = $existing_term->$field;
          }
        }
      }
    }

    // Default to bundle if no vocabulary machine name provided
    if (!isset($term->vocabulary_machine_name)) {
      $term->vocabulary_machine_name = $this->bundle;
    }
    // vid is required
    if (empty($term->vid)) {
      static $vocab_map = array();
      if (!isset($vocab_map[$term->vocabulary_machine_name])) {
        // The keys of the returned array are vids
        $vocabs = taxonomy_vocabulary_load_multiple(array(),
          array('machine_name' => $term->vocabulary_machine_name));
        $vids = array_keys($vocabs);
        if (isset($vids[0])) {
          $vocab_map[$term->vocabulary_machine_name] = $vids[0];
        }
        else {
          $migration->saveMessage(t('No vocabulary found with machine_name !name',
            array('!name' => $term->vocabulary_machine_name)));
          return FALSE;
        }
      }
      $term->vid = $vocab_map[$term->vocabulary_machine_name];
    }
    // Look up parent name if provided
    if (isset($term->parent_name)) {
      // Look for the name in the same vocabulary.
      // Note that hierarchies may have multiples of the same name...
      $terms = taxonomy_term_load_multiple(array(),
        array('name' => trim($term->parent_name), 'vid' => $term->vid));
      $tids = array_keys($terms);
      $term->parent = $tids[0];
      unset($term->parent_name);
    }
    if (!isset($term->parent)) {
      $term->parent = array(0);
    }
    if (!isset($term->format)) {
      $term->format = $this->inputFormat;
    }

    $this->prepare($migration, $term, $row);

    migrate_instrument_start('taxonomy_term_save');
    $status = taxonomy_term_save($term);
    migrate_instrument_stop('taxonomy_term_save');
    $this->complete($migration, $term, $row);
    $return = isset($term->tid) ? array($term->tid) : FALSE;
    return $return;
  }
}
