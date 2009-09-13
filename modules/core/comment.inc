<?php
// $Id$

/**
 * @file
 * Implementation of comment destination handling
 */

function comment_migrate_types() {
  $types = array('comment' => t('Comment'));
  return $types;
}

/**
 * Enter description here...
 *
 * @param unknown_type $type
 * @return unknown
 */
function comment_migrate_fields_comment($type) {
  $fields = array(
    'subject' => t('Comment: Title'),
    'comment' => t('Comment: Body'),
    'name' => t('Comment: Author'),
    'homepage' => t("Comment: Author's website"),
    'timestamp' => t('Comment: Post date'),
    'nid' => t('Comment: Node'),
    'uid' => t('Comment: User'),
    'pid' => t('Comment: Parent ID'),
  );

  return $fields;
}

function comment_migrate_delete_comment($cid) {
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

function comment_migrate_import_comment($tblinfo, $row) {
  $comment = array();
  $errors = array();
  $sourcekey = $tblinfo->sourcekey;

  foreach ($tblinfo->fields as $destfield => $values) {
    if ($values['srcfield'] && $row->$values['srcfield']) {
      $comment[$destfield] = $row->$values['srcfield'];
    }
    else {
      $comment[$destfield] = $values['default_value'];
    }
  }

  // Prepare the comment for import. We could have use migrate_invoke_all()
  // but unfortunately arguments are passed by value
  foreach (module_list() as $module_name) {
    $function = $module_name .'_migrate_prepare_comment';
    if (function_exists($function)) {
      timer_start($function);
      $errors = array_merge($errors, (array)$function($comment, $tblinfo, $row));
      timer_stop($function);
    }
  }

  /*
   * Validation: should probably create a validation hook instead
   */
  if (!$comment['nid']) {
    $errors[] = migrate_message(t('No node found'));
  }
  if (!$comment['name']) {
    $commentuser = user_load(array('uid' => $comment['uid']));
    $comment['name'] = $commentuser->name;
  }
  if (!$comment['name']) {
    $errors[] = migrate_message(t('No user id or name found'));
  }

  // Do our best to interpret timestamps
  if ($comment['timestamp']) {
    $comment['timestamp'] = _migrate_valid_date($comment['timestamp']);
    if ($comment['timestamp'] <= 0) {
      $errors[] = migrate_message(t('Provided timestamp is invalid'));
    }
  }

  //TODO: we should probably check actual errors, some may be info messages.
  if (count($errors) == 0) {
    $cid = comment_save($comment);
  }

  //TODO: replace this with the migrate_destination_invoke_all();
  foreach (module_list() as $module_name) {
    $function = $module_name .'_migrate_complete_comment';
    if (function_exists($function)) {
      timer_start($function);
      $errors = array_merge($errors, (array)$function($cid, $tblinfo, $row));
      timer_stop($function);
    }
  }

  return $errors;
}

function comment_migrate_complete_comment(&$cid, $tblinfo, $row) {
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
              (sourceid, destid)
              VALUES(%d, %d)",
             $row->$sourcekey, $cid);
  }
  else {
    $errors[] = migrate_message(t('Comment not saved'));
  }
  return $errors;
}

