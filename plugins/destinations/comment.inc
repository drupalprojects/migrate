<?php
// $Id$

/**
 * @file
 * Support for comment destinations.
 */

// TODO:
// Make sure this works with updates, explicit destination keys

/**
 * Destination class implementing migration into comments.
 */
class MigrateDestinationComment extends MigrateDestinationEntity {
  /**
   * Basic initialization
   *
   * @param string $bundle
   *  A.k.a. the content type (page, article, etc.) of the ... comment?.
   * @param string $language
   *  Language of the node content.
   * @param string $text_format
   *  Default text format for fields on this comment. Can be overridden.
   */
  public function __construct($bundle, $language = NULL, $text_format = NULL) {
    // PK definition, for use in constructing map/message tables
    $destination_key = array(
      'cid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'ID of destination entity',
      ),
    );
    parent::__construct($destination_key, 'comment', $bundle, $language, $text_format);
    $this->supportsBulkRollback = TRUE;
  }

  /**
   * Returns a list of fields available to be mapped for comments attached to
   * a particular bundle (node type)
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $fields = array();
    // First the core (comment table) properties
    $fields['cid'] = t('Comment: Existing comment ID');
    $fields['pid'] = t('Comment: Parent (by Drupal ID)');
    $fields['nid'] = t('Comment: Node (by Drupal ID)');
    $fields['uid'] = t('Comment: User (by Drupal ID)');
    $fields['user_name'] = t('Comment: User (by username)');
    $fields['user_email'] = t('Comment: User (by email address');
    $fields['subject'] = t('Comment: Subject');
    $fields['hostname'] = t('Comment: Hostname/IP address');
    $fields['created'] = t('Comment: Created timestamp');
    $fields['changed'] = t('Comment: Modified timestamp');
    $fields['status'] = t('Comment: Status');
    $fields['thread'] = t('Comment: Thread');   // ???
    $fields['name'] = t('Comment: User name (not username)');
    $fields['mail'] = t('Comment: Email address');
    $fields['homepage'] = t('Comment: Homepage');
    $fields['language'] = t('Comment: Language');

    // Then add any fields attached to comments
    $field_instance_info = field_info_instances($this->entityType, $this->bundle);
    foreach ($field_instance_info as $machine_name => $instance) {
      $field_info = field_info_field($machine_name);

      $fields[$machine_name] = t('Comment:') . ' ' . $instance['label'] .
        ' (' . $field_info['type'] . ')';
    }

    // Then add in anything provided by handlers
    $fields += migrate_handler_invoke_all('Comment', 'fields');

    return $fields;
  }

  /**
   * Delete a batch of comments at once.
   *
   * @param $migration
   *  Controlling migration object. Unused here.
   * @param $cids
   *  Array of comment IDs to be deleted.
   */
  public function bulkRollback(Migration $migration, array $cids) {
    migrate_instrument_start('comment_delete_multiple');
    $result = comment_delete_multiple($cids);
    migrate_instrument_stop('comment_delete_multiple');
    return $result;
  }

  /**
   * Import a single comment.
   *
   * @param $migration
   *  Controlling migration object. Passed through to prepare/complete handlers.
   * @param $comment
   *  Comment object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields (cid only in this case) of the comment that was saved if
   *  successful. FALSE on failure.
   */
  public function import(Migration $migration, stdClass $comment, stdClass $row) {
    // Handle update case
    if (isset($row->migrate_map_destid1)) {
      $comment->cid = $row->migrate_map_destid1;
    }

    // Set some default properties.
    $comment = (array) $comment;
    $comment += array(
      'language' => $this->language,
      'node_type' => $this->bundle,
      'subject' => '',
      'uid' => 0,
      'cid' => 0,
      'pid' => 0,
    );
    $comment = (object) $comment;

    $this->prepare($migration, $comment, $row);
    // Make sure we have a nid
    if (!isset($comment->nid) || !$comment->nid) {
      throw new MigrateException(t('No node ID provided for comment'));
    }
    migrate_instrument_start('comment_save');
    comment_save($comment);
    migrate_instrument_stop('comment_save');
    $this->complete($migration, $comment, $row);
    $return = isset($comment->cid) ? array($comment->cid) : FALSE;
    return $return;
  }
}
