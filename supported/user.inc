<?php
// $Id$

/**
 * @file
 * Implementation of user destination handling
 * TODO: Support for core profile module
 */


function migrate_migrate_destination_fields_user($type) {
  $fields = array(
    'name' => t('User: Username'),
    'pass' => t('User: Password'),
    'mail' => t('User: Email address'),
    'created' => t('User: Account created'),
    'access' => t('User: Account last accessed'),
    'login' => t('User: Last logged in'),
  );
  
  // if content_profile is present, get any CCK fields attached to it
  if (module_exists('content_profile')) {
    // @TODO: Note hard-coded profile node type...
    $profile_fields = migrate_invoke_all('destination', 'fields', 'node', 'profile');
    
    foreach ($profile_fields as $key => $value) {
      if (!preg_match('/^field_/', $key)) {
        unset($profile_fields[$key]);
      } else {
        $profile_fields[$key] = t('Profile: ').$value;
      }
    }
    $fields = array_merge($userfields, $profile_fields);
  }
  return $fields;
}

function migrate_migrate_destination_delete_user($uid) {
  timer_start('user_delete');
  user_delete(array(), $uid);
  timer_stop('user_delete');
}

function migrate_migrate_destination_import_user($tblinfo, $row) {
  $use_profile = module_exists('content_profile');

  $sourcekey = $tblinfo->sourcekey;
  // Begin building user object...
  $newuser = array();
  // Default to enabled
  $newuser['status'] = 1;
  $newuser['roles'][$tblinfo->desttype] = $tblinfo->desttype;
  
  if ($use_profile) {
    // ...and, if applicable content_profile node object
    $node = new StdClass;
    $node->type = 'profile';
  }

  foreach ($tblinfo->fields as $destfield => $values) {
    if ($values['srcfield'] && isset($row->$values['srcfield'])) {
      $newvalue = $row->$values['srcfield'];
    } else {
      $newvalue = $values['default_value'];
    }
    if ($use_profile && preg_match('/^field_/', $destfield)) {
      $node->$destfield = $newvalue;
    } else {
      // Make sense of timestamp fields
      if ($destfield == 'created' || $destfield == 'access' ||
          $destfield == 'login') {
        $newvalue = _migrate_valid_date($newvalue);
      }
      $newuser[$destfield] = $newvalue;
    }
  }
  
  // Prepare the user for import. We could have used module_invoke_all,
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
  
  $success = TRUE;
  foreach ($errors as $error) {
    if ($error['level'] != MIGRATE_MESSAGE_INFORMATIONAL) {
      $success = FALSE;
      break;
    }
  }
  
  if ($success) {
    // If email_registration is being used, it will generate a unique username
    // from the email address in its user hook - we need to put a name in here
    // so it passes previous validation
    if (module_exists('email_registration')) {
      $newuser['name'] = 'dlksflkaj';
      $username = $newuser['username'];
      unset($newuser['username']);
    }
    
    timer_start('user_save');
    $account = user_save((object)$newuser, $newuser);
    timer_stop('user_save');

    // Call completion hooks, for any processing which needs to be done after user_save
    timer_start('user completion hooks');
    foreach (module_list() as $module_name) {
      $function = $module_name .'_migrate_destination_complete_user';
      if (function_exists($function)) {
        timer_start($function);
        $errors = array_merge($errors, (array)$function($account, $tblinfo, $row));
        timer_stop($function);
      }
    }
    timer_stop('user completion hooks');
    
    if ($account) {
      db_query("INSERT INTO {" . $tblinfo->maptable . "}
                ($sourcekey, userid, mcsid)
                VALUES(%d, %d, %d)",
               $row->$sourcekey, $account->uid, $tblinfo->mcsid);
      // email_registration module overrides username, set the one we want here
      if (module_exists('email_registration') && $username) {
        db_query("UPDATE {users} SET name = '%s' WHERE uid = '%s'", $username, $account->uid);
        $account->name = $username;
        unset($newuser['username']);
      }
      if ($use_profile) {
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
  }
  return $errors;
}

function migrate_migrate_destination_xlat_user($uid) {
  return "user/$uid";
}
