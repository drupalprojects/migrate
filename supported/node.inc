<?php
/* $Id$ */

/**
 * Implementation of hook_migrate_fields().
 */
function node_migrate_destination_fields_node($type) {
  $fields = array();
  if (user_access('administer nodes')) {
    $fields = array(
      'name' => t('Node: Authored by'),
      'date' => t('Node: Authored on'),
      'updated' => t('Node: Last updated on'),
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

  return $fields;
}

/**
 * Implementation of hook_migrate_prepare().
 */
function node_migrate_destination_prepare_node(&$node, $tblinfo, $row) {
  global $user;

  $type_info = node_get_types('type', $node->type);
  $pkcol = $tblinfo->pkcol;

  $errors = array();

  // 1. User name and uid.
  if (isset($node->uid)) {
    $account = user_load(array('uid' => $node->uid));
    $node->name = $account->name;
  } elseif (isset($node->name)) {
    // We have a mapped username.
    $account = user_load(array('name' => $node->name));
    // @TODO: What if no match...
    $node->uid = $account->uid;
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
      $errors[] = array($row->$pkcol => t('The date %date is not a valid date.', 
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
      $errors[] = array($row->$pkcol => t('The date %date is not a valid date.', 
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

  if (isset($node->updated)) {
    // We have a mapped updated date.
    if (empty($node->updated)) {
      unset($node->updated);
    }
    else if (($date = _migrate_valid_date($node->updated)) > -1) {
      $node->updated = $date;
    }
    else {
      $errors[] = array($row->$pkcol => t('The date %date is not a valid date.', 
        array('%date' => theme('placeholder', $node->updated))));
      unset($node->updated);
    }
  }

  if (!isset($node->updated)) {
    // We still don't have a date yet, use the current time.
    $node->updated = time();
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
//    $errors[] = array($row->$pkcol => t('You need to provide a non-empty title.'));
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
    $node->teaser = trim($node->teaser);
    $node->body = trim($node->body);
    if ($node->teaser && !$node->body) {
      $node->body = $node->teaser;
    } elseif ($node->body && !$node->teaser) {
      // Teaser not automatically generated
      $node->teaser = node_teaser($node->body);
    }
  }
  return $errors;
}

/**
 * Implementation of hook_migrate_global().
 */
function node_migrate_global($type, $global_values) {
  global $user;
  $globals = $global_values['migrate_node'];

  if (isset($globals['options'])) {
    $options = array();
    foreach ($globals['options'] as $option => $value) {
      if ($value) {
        $options[] = $option;
      }
    }
    $globals['options'] = $options;
  }
  else {
    $defaults = array('status', 'promote');
    $globals['options'] = variable_get('node_options_'. $type, $defaults);
  }

  if (!isset($globals['author'])) {
    $globals['author'] = array(
      'name' => $user->name,
      'date' => '',
    );
  }

  // Create a dummy node and get the node form.
  $node = array(
    'type' => $type,
    'date' => date('r'),
    'name' => $user->name,
    'uid' => $user->uid,
  );
  $node = array_merge($node, $globals['author']);
  foreach ((array)$globals['options'] as $option) {
    $node[$option] = 1;
  }
  $node = (object)$node;
  
  module_load_include('inc', 'node', 'node.pages');
  $node_form = node_form($form_state, $node);

  // We only care about a part of this node_form.
  $form = array();
  $form['migrate_node'] = array(
    '#type' => 'fieldset',
    '#title' => t('Node options'),
    '#description' => t('Select the options you want to assign to all imported nodes unless specifically set otherwise in the CSV file'),
    '#tree' => TRUE,
  );
  foreach (array('author', 'options') as $fieldset) {
    if (isset($node_form[$fieldset])) {
      $form['migrate_node'][$fieldset] = $node_form[$fieldset];
      if ($fieldset == 'options') {
        // The workflow options are overwritten in node_form_array()
        // because $node->nid is not set.
        foreach (element_children($form['migrate_node']['options']) as $key) {
          $form['migrate_node']['options'][$key]['#default_value'] = $node->$key;
        }
      }
    }
  }
  return $form;
}
