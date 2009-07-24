<?php
/* $Id$ */

/**
 * @file
 * Implementation of node destination handling
 */

function node_migrate_destination_types() {
  static $nodetypes = NULL;

  if (!isset($nodetypes)) {
    $nodetypes = array();
    foreach (node_get_types() as $type => $info) {
      $nodetypes['node/' . $type] = t('Node: ') . $info->name;
    }
    asort($nodetypes);
  }
  return $nodetypes;
}

function node_migrate_destination_delete_node($nid) {
  // If we've created associations in {upload} between imported nodes
  // and files directly copied to the Drupal files directory (e.g. using
  // the auditfiles module), deleting the nodes deletes the files (not
  // what we typically want when going through clear/import cycles). We
  // short-circuit this behavior by explicitly deleting the {upload}
  // table entries for this node, so the upload module leaves the files alone.
  if (db_table_exists('upload')) {
    db_query("DELETE FROM {upload} WHERE nid=%d", $nid);
  }
  node_delete($nid);
}

function node_migrate_destination_import_node($tblinfo, $row) {
  $node = (object)array();
  $node->type = $tblinfo->desttype;
  // Make sure the default comment settings are applied
  $function = 'comment_nodeapi';
  if (function_exists($function)) {
    $function($node, "prepare");
  }

  foreach ($tblinfo->fields as $destfield => $values) {
    if ($values['srcfield'] && $row->$values['srcfield']) {
      $node->$destfield = $row->$values['srcfield'];
    } else {
      $node->$destfield = $values['default_value'];
    }
  }

  // Prepare the node for import.
  $errors = migrate_destination_invoke_all('prepare_node', $node, $tblinfo, $row);

  $success = TRUE;
  foreach ($errors as $error) {
    if ($error['level'] != MIGRATE_MESSAGE_INFORMATIONAL) {
      $success = FALSE;
      break;
    }
  }
  if ($success) {
    timer_start('node_save');
    // node_save() unconditionally sets the changed timestamp to time() - we need
    // to force the migrated value if present
    $changed = $node->changed;
    node_save($node);
    $node->changed = $changed;
    drupal_write_record('node', $node, 'nid');
    timer_stop('node_save');
    // Call completion hooks, for any processing which needs to be done after node_save
    timer_start('node completion hooks');
    $errors = migrate_destination_invoke_all('complete_node', $node, $tblinfo, $row);
    timer_stop('node completion hooks');

    $sourcekey = $tblinfo->sourcekey;
    // @TODO: Check first for existence, we may have updated an existing node
    // @TODO: Back out a new revision?
    migrate_add_mapping($tblinfo->mcsid, $row->$sourcekey, $node->nid);
  }
  return $errors;
}

function node_migrate_destination_xlat_node($nid) {
  return "node/$nid";
}

/**
 * Implementation of hook_migrate_fields().
 */
function node_migrate_destination_fields_node($type) {
  $fields = array();
  if (user_access('administer nodes')) {
    $fields = array(
      'name' => t('Node: Authored by'),
      'date' => t('Node: Authored on'),
      'changed' => t('Node: Last updated on'),
      'status' => t('Node: Published'),
      'moderate' => t('Node: In moderation queue'),
      'promote' => t('Node: Promoted to front page'),
      'sticky' => t('Node: Sticky at top of lists'),
      'revision' => t('Node: Create new revision'),
    );
  }

  $type_info = node_get_types('type', $type);

  if ($type_info->has_title) {
    $fields['title'] = t('Node: ').$type_info->title_label;
  }
  if ($type_info->has_body) {
    $fields['body'] = t('Node: ').$type_info->body_label;
    $fields['teaser'] = t('Node: Teaser');
  }

  if (module_exists('path') && (user_access('create url aliases') || user_access('administer url aliases'))) {
    $fields['path'] = t('Path: URL Alias');
  }

  return $fields;
}

/**
 * Implementation of hook_migrate_prepare().
 */
function node_migrate_destination_prepare_node(&$node, $tblinfo, $row) {
  global $user;

  $type_info = node_get_types('type', $node->type);

  $errors = array();

  // 1. User name and uid.
  if (isset($node->uid)) {
    $account = user_load(array('uid' => $node->uid));
    $node->name = $account->name;
  } elseif (isset($node->name)) {
    // We have a mapped username.
    if ($account = user_load(array('name' => $node->name))) {
      $node->uid = $account->uid;
    } elseif ($account = user_load(array('mail' => $node->name))) {
      $node->uid = $account->uid;
    }
    // If no valid incoming user, the node owner will end up being the current user
  } elseif ($user->uid > 0) {
    $node->uid = $user->uid;
    $node->name = $user->name;
  } else {
    // Default to admin user
    $node->uid = 1;
    $account = user_load(array('name' => $node->uid));
    $node->name = $account->name;
  }

  // 2. Creation date.
  if (isset($node->date)) {
    // We have a mapped date.
    if (empty($node->date)) {
      unset($node->date);
    }
    else if (($date = _migrate_valid_date($node->date)) > -1) {
      $node->date = $date;
    }
    else {
      migrate_message(t('The created date %date is not a valid date.',
        array('%date' => $node->date)));
      unset($node->date);
    }
  }

  if (!isset($node->date) && isset($author)) {
    // We don't have a date yet (not mapped or error above), but we
    // do have some global options.
    if ($author['date'] == '') {
      $node->date = time();
    }
    else if (($date = _migrate_valid_date($author['date'])) > -1) {
      $node->date = $date;
    }
    else {
      migrate_message(t('The date %date is not a valid date.',
        array('%date' => $author['date'])));
    }
  }

  if (!isset($node->date)) {
    // We still don't have a date yet, use the current time.
    $node->date = time();
  }

  $node->created = $node->date;
  $node->date = format_date($node->created, 'custom', 'Y-m-d H:i:s O');

  // 3. Last updated
  if (isset($node->changed)) {
    // We have a mapped changed date.
    if (empty($node->changed)) {
      unset($node->changed);
    }
    else if (($date = _migrate_valid_date($node->changed)) > -1) {
      $node->changed = $date;
    }
    else {
      $errors[] = migrate_message(t('The changed date %date is not a valid date.',
        array('%date' => theme('placeholder', $node->changed))));
      unset($node->changed);
    }
  }

  if (!isset($node->changed)) {
    // We still don't have a date yet, use the current time.
    $node->changed = time();
  }

  // 4. Options (published, promoted, sticky, moderated, new revision).
  if (!isset($options)) {
    $options = variable_get('node_options_'. $node->type, array('status', 'promote'));
  }
  $all_options = array('status', 'moderate', 'promote', 'sticky', 'revision');

  foreach ($all_options as $key) {
    if (isset($node->$key) && strlen($node->$key) > 0) {
      // If the field was mapped, use that value.
      $node->$key = ($node->$key ? 1 : 0);
    }
    else {
      // If not, use the global option.
      $node->$key = (isset($options[$key]) ? $options[$key] : 0);
    }
  }

  // 5. Title.
  if ($type_info->has_title && (!isset($node->title) || empty($node->title))) {
    $node->title = t('{Empty title}');
//    $errors[] = migrate_messaget('You need to provide a non-empty title.'));
  }

  // Strip tags from the title
  if (isset($node->title)) {
    $node->title = strip_tags($node->title);
  }

  // 6. Input Format
  // Default to Full HTML
  $node->format = 2;

  // 7. Body/teaser
  if ($type_info->has_body) {
    // If incoming data has a teaser and no body, copy the teaser into the body
    if (isset($node->teaser)) {
      $node->teaser = trim($node->teaser);
    } else {
      $node->teaser = '';
    }
    if (isset($node->body)) {
      $node->body = trim($node->body);
    } else {
      $node->body = '';
    }
    if ($node->teaser && !$node->body) {
      $node->body = $node->teaser;
    } elseif ($node->body && !$node->teaser) {
      // Teaser not automatically generated
      $node->teaser = node_teaser($node->body);
    }
  }
  return $errors;
}
