<?php
// $Id$

/**
 * @file
 * Support for node destinations.
 */

// TODO:
// Make sure this works with updates, explicit destination keys

/**
 * Destination class implementing migration into nodes.
 */
class MigrateDestinationNode extends MigrateDestinationEntity {
  static public function getKeySchema() {
    return array(
      'nid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'ID of destination node',
      ),
    );
  }

  /**
   * Basic initialization
   *
   * @param string $bundle
   *  A.k.a. the content type (page, article, etc.) of the node.
   * @param string $language
   *  Default language for nodes created via this destination class.
   */
  public function __construct($bundle, $language = NULL, $text_format = NULL) {
    parent::__construct('node', $bundle, $language, $text_format);
  }

  /**
   * Returns a list of fields available to be mapped for the node type (bundle)
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $fields = array();
    // First the core (node table) properties
    $fields['nid'] = t('Node: Existing node ID');
    $fields['uid'] = t('Node: Authored by (uid)');
    $fields['created'] = t('Node: Created timestamp');
    $fields['changed'] = t('Node: Modified timestamp');
    $fields['body'] = t('Node: Body');
    $fields['teaser'] = t('Node: Teaser');
    $fields['status'] = t('Node: Published');
    $fields['promote'] = t('Node: Promoted to front page');
    $fields['sticky'] = t('Node: Sticky at top of lists');
    $fields['revision'] = t('Node: Create new revision');
    $fields['language'] = t('Node: Language (fr, en, ...)');

    $node_type = node_type_load($this->bundle);
    if ($node_type->has_title) {
      $fields['title'] = t('Node:') . ' ' . $node_type->title_label;
    }

    // Then add any fields attached to this node type (bundle)
    // TODO: Move to migrate_extras
    if (module_exists('content')) {
      $field_instance_info = content_fields();
      foreach ($field_instance_info as $machine_name => $instance) {
        if ($instance['type_name'] == $this->bundle) {
          $fields[$machine_name] = t('Node:') . ' ' . $instance['widget']['label'] .
            ' (' . $instance['type'] . ')';
        }
      }
    }

    // Then add in anything provided by handlers
    $fields += migrate_handler_invoke_all('Node', 'fields');

    return $fields;
  }

  /**
   * Delete a batch of nodes at once.
   *
   * @param $migration
   *  Controlling migration object. Unused here.
   * @param $nids
   *  Array of node IDs to be deleted.
   */
  public function bulkRollback(Migration $migration, array $nids) {
    migrate_instrument_start('node_delete_multiple');
    node_delete_multiple($nids);
    migrate_instrument_stop('node_delete_multiple');
  }

  /**
   * Import a single node.
   *
   * @param $migration
   *  Controlling migration object. Passed through to prepare/complete handlers.
   * @param $node
   *  Node object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields (nid only in this case) of the node that was saved if
   *  successful. FALSE on failure.
   */
  public function import(Migration $migration, stdClass $node, stdClass $row) {
    // Handle update case
    if (isset($row->migrate_map_destid1)) {
      $node->nid = $row->migrate_map_destid1;
    }

    // Set type before invoking prepare handlers - they may take type-dependent actions.
    $node->type = $this->bundle;

    // Save changed timestamp and set it later, node_save sets it unconditionally
    if (isset($node->changed)) {
      $changed = MigrationBase::timestamp($node->changed);
    }

    if (isset($node->created)) {
      $node->created = MigrationBase::timestamp($node->created);
    }

    // Invoke migration prepare handlers
    $this->prepare($migration, $node, $row);
    $node->revision = 0; // Always create a new revision.
    migrate_instrument_start('node_save');
    node_save($node);
    migrate_instrument_stop('node_save');

    // Unfortunately, http://drupal.org/node/722688 was not accepted, so fix
    // the changed timestamp here
    if (isset($changed) && isset($node->nid)) {
      db_update('node')
        ->fields(array('changed' => $changed))
        ->condition('nid', $node->nid)
        ->execute();
      $node->changed = $changed;
    }
    $this->complete($migration, $node, $row);

    // Clear the node cache periodically
    static $count = 0;
    if ($count++ > 100) {
      node_load('foo', NULL, TRUE);
      $count = 0;
    }

    $return = isset($node->nid) ? array($node->nid) : FALSE;
    return $return;
  }
}
