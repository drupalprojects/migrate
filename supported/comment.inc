<?php
// $Id$

/**
 * @file
 * Implementation of comment destination handling
 */

/**
 * Enter description here...
 *
 * @param unknown_type $type
 * @return unknown
 */
function migrate_migrate_destination_fields_comment($type) {
  $fields = array(
    'subject' => t('Comment: Title'),
    'comment' => t('Comment: Body'),
    'name' => t('Comment: Author'),
    'homepage' => t("Comment: Author's website"),
    'timestamp' => t('Comment: Post date'),
    'nid' => t('Comment: Node'),
    'uid' => t('Comment: User'),
  );

  return $fields;
}

function migrate_migrate_destination_delete_comment($cid) {
  $path = drupal_get_path('module', 'comment') . '/comment.admin.inc';
  include_once($path);
  // Backdoor deletion - query stolen from comment_delete()
  $form = array();
  $form_state = array();
  $form['#comment'] = db_fetch_object(db_query(
    'SELECT c.*, u.name AS registered_name, u.uid 
     FROM {comments} c 
     INNER JOIN {users} u ON u.uid = c.uid 
     WHERE c.cid = %d', 
    $cid));
  comment_confirm_delete_submit($form, $form_state);
}

function migrate_migrate_destination_import_comment($tblinfo, $row) {
  $comment = array();
  $errors = array();
  $sourcekey = $tblinfo->sourcekey;
  
  foreach ($tblinfo->fields as $destfield => $values) {
     if ($values['srcfield'] && $row->$values['srcfield']) {
      $comment[$destfield] = $row->$values['srcfield'];
    } else {
      $comment[$destfield] = $values['default_value'];
    }
  }
  
  // Prepare the comment for import. We could have use migrate_invoke_all()
  // but unfortunately arguments are passed by value
  foreach (module_list() as $module_name) {
    $function = $module_name .'_migrate_destination_prepare_comment';
    if (function_exists($function)) {
      timer_start($function);
      $errors = array_merge($errors, (array)$function($comment, $tblinfo, $row));
      timer_stop($function);
    }
  }
  
  // nid and uid are initially populated with the source file's ID - translate to 
  // Drupal IDs
  if ($comment['nid']) {
    $comment['nid'] = _migrate_xlat_get_new_id('node', $comment['nid']);
  }
  if (!$comment['nid']) {
    $errors[] = array(
      'sourceid' => $row->$sourcekey,
      'message' => t('No node fond'),
    );
  }
  if ($comment['uid']) {
    $comment['uid'] = _migrate_xlat_get_new_id('user', $comment['uid']);
    if (!$comment['name']) {
      $commentuser = user_load(array('uid' => $comment['uid']));
      $comment['name'] = $commentuser->name;
    }
  }
  if (!$comment['name']) {
    $errors[] = array(
      'sourceid' => $row->$sourcekey,
      'message' => t('No user id or name found'),
    );
  }
  
  // Do our best to interpret timestamps
  if ($comment['timestamp']) {
    $comment['timestamp'] = _migrate_valid_date($comment['timestamp']);
    if ($comment['timestamp'] <= 0) {
      $errors[] = array(
        'sourceid' => $row->$sourcekey,
        'message' => t('Provided timestamp is invalid'),
      );
    }
  }
  
  if (count($errors) == 0) {
    $cid = comment_save($comment);
    if ($cid) {
      // comment_save() always saves time() as the timestamp, override if we have a value
      if ($comment['timestamp']) {
        db_query("UPDATE {comments}
                  SET timestamp=%d
                  WHERE cid=%d",
                 $comment['timestamp'], $cid);
      }
      $sourcekey = $tblinfo->sourcekey;
      //@TODO: Factor out into caller
      db_query("INSERT INTO {" . $tblinfo->maptable . "}
                ($sourcekey, commentid, mcsid)
                VALUES(%d, %d, %d)",
               $row->$sourcekey, $cid, $tblinfo->mcsid);
    } else {
      $errors[] = array($row->$sourcekey => t('Comment not saved'));
      $errors[] = array(
        'sourceid' => $row->$sourcekey,
        'message' => t('Comment not saved'),
      );
    }
  }
  return $errors;
}
