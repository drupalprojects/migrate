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
    $fields['is_new'] = t('Node: Indicates a new node with the specified nid should be created');
    $fields['uid'] = t('Node: Authored by (uid)');
    $fields['created'] = t('Node: Created timestamp');
    $fields['changed'] = t('Node: Modified timestamp');
    $fields['status'] = t('Node: Published');
    $fields['promote'] = t('Node: Promoted to front page');
    $fields['sticky'] = t('Node: Sticky at top of lists');
    $fields['revision'] = t('Node: Create new revision');
    $fields['language'] = t('Node: Language (fr, en, ...)');

    $node_type = node_type_load($this->bundle);
    if ($node_type->has_title) {
      $fields['title'] = t('Node:') . ' ' . $node_type->title_label;
    }

    // Then add in anything provided by handlers
    $fields += migrate_handler_invoke_all('Node', 'fields', $this->entityType, $this->bundle);

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

    // Set some required properties.
    if (!isset($node->language)) {
      $node->language = $this->language;
    }

    // Set type before invoking prepare handlers - they may take type-dependent actions.
    $node->type = $this->bundle;

    // Apply defaults, allow standard node prepare hooks to fire.
    // node_object_prepare() will blow these away, so save them here and
    // stuff them in later if need be.
    if (isset($node->created)) {
      $created = $node->created;
    }
    else {
      //To keep node_object_prepare() from choking
      $node->created = REQUEST_TIME;
    }
    if (isset($node->changed)) {
      $changed = $node->changed;
    }
    if (isset($node->uid)) {
      $uid = $node->uid;
    }
    node_object_prepare($node);
    if (isset($created)) {
      $node->created = $created;
    }
    // No point to resetting $node->changed here, node_save() will overwrite it
    if (isset($uid)) {
      $node->uid = $uid;
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
    $return = isset($node->nid) ? array($node->nid) : FALSE;
    return $return;
  }
}
