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
    $fields = array_merge($fields, $profile_fields);
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
    // Generate a random username if none was provided (presumably we
    // are migrating from a system using email addresses as account names)
    if (!isset($newuser['name']) || !$newuser['name']) {
      // There is a tiny risk that the generated name will not be unique
      $newuser['name'] = user_password();
    }

    // This field appears to be obsolete, but little cost to setting it...
    if (!isset($newuser['init'])) {
      $newuser['init'] = $newuser['mail'];
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

/**
 * Implementation of hook_email_registration_name()
 * 
 * If email_registration is in use, it overwrites any explicit username
 * with one generated from the email address. If you install the patch at 
 * http://drupal.org/node/247717, this hook will short-circuit that and
 * force the username set in the migration process (possibly randomly
 * generated above).
 * 
 * @param $edit
 *  The values we passed to user_save().
 * @param $account
 *  The user account as created by user_save().
 * @return 
 *  The username we've already set - returning this prevents
 *  email_registration from overwriting it.
 */
function migrate_email_registration_name($edit, $account) {
  return $edit['name'];
}
