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
    $fields += migrate_handler_invoke_all('Entity', 'fields', $this->entityType, $this->bundle);
    $fields += migrate_handler_invoke_all('Node', 'fields', $this->entityType, $this->bundle);

    return $fields;
  }

  /**
   * Delete a batch of nodes at once.
   *
   * @param $nids
   *  Array of node IDs to be deleted.
   */
  public function bulkRollback(array $nids) {
    migrate_instrument_start('node_delete_multiple');
    node_delete_multiple($nids);
    migrate_instrument_stop('node_delete_multiple');
  }

  /**
   * Import a single node.
   *
   * @param $node
   *  Node object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields (nid only in this case) of the node that was saved if
   *  successful. FALSE on failure.
   */
  public function import(stdClass $node, stdClass $row) {
    // Updating previously-migrated content?
    $migration = Migration::currentMigration();
    if (isset($row->migrate_map_destid1)) {
      if (isset($node->nid)) {
        if ($node->nid != $row->migrate_map_destid1) {
          throw new MigrateException(t("Incoming nid !nid and map destination nid !destid1 don't match",
            array('!nid' => $node->nid, '!destid1' => $row->migrate_map_destid1)));
        }
      }
      else {
        $node->nid = $row->migrate_map_destid1;
      }
      // Get the existing vid, so updates don't generate notices
      $node->vid = db_select('node', 'n')
                   ->fields('n', array('vid'))
                   ->condition('nid', $node->nid)
                   ->execute()
                   ->fetchField();
    }
    if ($migration->getSystemOfRecord() == Migration::DESTINATION) {
      if (!isset($node->nid)) {
        throw new MigrateException(t('System-of-record is DESTINATION, but no destination nid provided'));
      }
      $old_node = node_load($node->nid);
      if (!isset($node->created)) {
        $node->created = $old_node->created;
      }
      if (!isset($node->vid)) {
        $node->vid = $old_node->vid;
      }
      if (!isset($node->status)) {
        $node->status = $old_node->status;
      }
      if (!isset($node->uid)) {
        $node->uid = $old_node->uid;
      }
    }

    // Set some required properties.
    // Set type before invoking prepare handlers - they may take type-dependent actions.
    $node->type = $this->bundle;

    if ($migration->getSystemOfRecord() == Migration::SOURCE) {
      if (!isset($node->language)) {
        $node->language = $this->language;
      }

      // Apply defaults, allow standard node prepare hooks to fire.
      // node_object_prepare() will blow these away, so save them here and
      // stuff them in later if need be.
      if (isset($node->created)) {
        $created = MigrationBase::timestamp($node->created);
      }
      else {
        // To keep node_object_prepare() from choking
        $node->created = REQUEST_TIME;
      }
      if (isset($node->changed)) {
        $changed = MigrationBase::timestamp($node->changed);
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
    }

    // Invoke migration prepare handlers
    $this->prepare($node, $row);
    $node->revision = 0; // Always create a new revision.
    migrate_instrument_start('node_save');
    node_save($node);
    migrate_instrument_stop('node_save');

    if (isset($node->nid)) {

      // Unfortunately, http://drupal.org/node/722688 was not accepted, so fix
      // the changed timestamp
      if (isset($changed)) {
        db_update('node')
          ->fields(array('changed' => $changed))
          ->condition('nid', $node->nid)
          ->execute();
        $node->changed = $changed;
      }

      // Potentially fix uid and timestamp in node_revisions.
      $query = db_update('node_revision')
                 ->condition('vid', $node->vid);
      if (isset($changed)) {
        $fields['timestamp'] = $changed;
      }
      if ($node->uid != $GLOBALS['user']->uid) {
        $fields['uid'] = $node->uid;
      }
      if (!empty($fields)) {
        // We actually have something to update.
        $query->fields($fields);
        $query->execute();
        $node->timestamp = $changed;
      }
    }

    $this->complete($node, $row);
    $return = isset($node->nid) ? array($node->nid) : FALSE;
    return $return;
  }
}
