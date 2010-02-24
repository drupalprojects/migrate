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

  /**
   * Basic initialization
   *
   * @param string $bundle
   *  A.k.a. the content type (page, article, etc.) of the node.
   * @param string $language
   *  Default language for nodes created via this destination class.
   */
  public function __construct($bundle, $language = LANGUAGE_NONE, $input_format = NULL) {
    // PK definition, for use in constructing map/message tables
    $destination_key = array(
      'nid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'ID of destination node',
      ),
    );
    parent::__construct($destination_key, 'node', $bundle, $language, $input_format);
    $this->supportsBulkRollback = TRUE;
  }

  /**
   * Returns a list of fields available to be mapped for the node type (bundle)
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping and
   *        addFieldMappingCallback)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $fields = array();
    // First the core (node table) properties
    $fields['nid'] = t('Node: Existing node ID');
    $fields['name'] = t('Node: Authored by (username)');
    $fields['uid'] = t('Node: Authored by (uid)');
    $fields['authorid'] = t('Node: Authored by (source ID)');
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

    // Then add any fields attached to this node type (bundle)
    // TODO: Factor out into entity.inc
    // TODO: Provide more details (like cardinality)
    $field_instance_info = field_info_instances($this->entityType, $this->bundle);
    foreach ($field_instance_info as $machine_name => $instance) {
      $field_info = field_info_field($machine_name);

      $fields[$machine_name] = t('Node:') . ' ' . $instance['label'] .
        ' (' . $field_info['type'] . ')';
    }

    // TODO: Then add in anything provided by handlers

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
  public function import(Migration $migration, stdClass $node, stdClass $row, array $arguments) {
    // Set some required properties.
    if (!isset($node->language)) {
      $node->language = $this->language;
    }

    // To keep node_object_prepare from choking
    if (!isset($node->created)) {
      $node->created = REQUEST_TIME;
    }

    // Set type before invoking prepare handlers - they may take type-dependent actions.
    $node->type = $this->bundle;

    // Apply defaults, allow standard node prepare hooks to fire
    module_load_include('pages.inc', 'node');
    // node_object_prepare() will blow these away, so save them here and
    // stuff them in later if need be
    if (isset($node->changed)) {
      $changed = $node->changed;
    }
    if (isset($node->uid)) {
      $uid = $node->uid;
    }
    node_object_prepare($node);
    if (isset($changed)) {
      $node->changed = $changed;
    }
    if (isset($uid)) {
      $node->uid = $uid;
    }

    // Look for Field API fields and handle appropriately
    $this->prepareFields($node, $arguments);
    // Invoke migration prepare handlers
    migrate_handler_invoke_all('Node', 'prepare', $migration, $node, $row);
    $node->revision = 0; // Always create a new revision.
    migrate_instrument_start('node_save');
    node_save($node);
    migrate_instrument_stop('node_save');
    migrate_handler_invoke_all('Node', 'complete', $migration, $node, $row);
    $return = isset($node->nid) ? array($node->nid) : FALSE;
    return $return;
  }
}
