<?php
// $Id$
/**
 * @file
 * Implementation of user destination handling
 */


function migrate_migrate_destination_fields_user($type) {
  // @TODO: Note hard-coded profile node type...
  $userfields = array(
    // Note user field is 'name', but this gets overridden from migrate_fields by the author
    // name for a node
    'username' => t('User: Username'),
    'pass' => t('User: Password'),
    'mail' => t('User: Email address'),
    'created' => t('User: Account created'),
    'access' => t('User: Account last accessed'),
    'login' => t('User: Last logged in'),
  );
  // We're only interested in the CCK fields in this context
  $profile_fields = migrate_invoke_all('destination', 'fields', 'node', 'profile');
  
  foreach ($profile_fields as $key => $value) {
    if (!preg_match('/^field_/', $key)) {
      unset($profile_fields[$key]);
    } else {
      $profile_fields[$key] = t('Profile: ').$value;
    }
  }
  $fields = array_merge($userfields, $profile_fields);
  return $fields;
}

function migrate_migrate_destination_delete_user($uid) {
  user_delete(array(), $uid);
}

function migrate_migrate_destination_import_user($tblinfo, $row) {
  $sourcekey = $tblinfo->sourcekey;
  // Begin building user object...
  $newuser = array();
  $newuser['status'] = 1;
  $newuser['roles'][$tblinfo->desttype] = $tblinfo->desttype;
  // ...and content_profile node object
  // @TODO: Generalize
  $node = new StdClass;
  $node->type = 'profile';

  foreach ($tblinfo->fields as $destfield => $values) {
    if ($values['srcfield'] && $row->$values['srcfield']) {
      $newvalue = $row->$values['srcfield'];
    } else {
      $newvalue = $values['default_value'];
    }
    if (preg_match('/^field_/', $destfield)) {
      $node->$destfield = $newvalue;
    } else {
      $newuser[$destfield] = $newvalue;
    }
  }
  
  // Prepare the user for import. We could have written the following loop
  // as: module_invoke_all('node_import_prepare', $node, $preview > 0);
  // but unfortunately module_invoke_all passes all arguments by value.
  $errors = array();
  foreach (module_list() as $module_name) {
    $function = $module_name .'_migrate_destination_prepare_user';
    if (function_exists($function)) {
      timer_start($function);
      $errors = array_merge($errors, (array)$function($newuser, $tblinfo, $row));
      timer_stop($function);
    }
  }
  
  // @TODO: We're depending on email_registration to replace the name
  $newuser['name'] = 'dlksflkaj';
  $username = $newuser['username'];
  unset($newuser['username']);
  $account = user_save(NULL, $newuser);
  if ($account) {
    db_query("INSERT INTO {" . $tblinfo->maptable . "}
              ($sourcekey, userid, mcsid)
              VALUES(%d, %d, %d)",
             $row->$sourcekey, $account->uid, $tblinfo->mcsid);
    // email_registration module overrides username, set the one we want here
    // @TODO: How do we generalize this case?
    if ($username) {
      db_query("UPDATE {users} SET name = '%s' WHERE uid = '%s'", $username, $account->uid);
      $account->name = $username;
    }
    unset($newuser['username']);
    // @TODO: From content_profile_registration_user_register_submit - generalize
    $node->uid = $account->uid;
    $node->title = $account->name;
    $node->name = $account->name;
    // Create the node.
    // Prepare the node for import. We could have written the following loop
    // as: module_invoke_all('node_import_prepare', $node, $preview > 0);
    // but unfortunately module_invoke_all passes all argumens by value.
    foreach (module_list() as $module_name) {
      $function = $module_name .'_migrate_destination_prepare_node';
      if (function_exists($function)) {
        $errors = array_merge((array)$errors, (array)$function($node, $tblinfo, $row));
      }
    }
    $node = node_submit($node);
    node_save($node);
  }
}

function migrate_migrate_destination_xlat_user($uid) {
  return "user/$uid";
}
