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
  
  return $fields;
}

function migrate_migrate_destination_delete_user($uid) {
  timer_start('user_delete');
  user_delete(array(), $uid);
  timer_stop('user_delete');
}

function migrate_migrate_destination_import_user($tblinfo, $row) {
  $sourcekey = $tblinfo->sourcekey;
  // Begin building user object...
  $newuser = array();
  // Default to enabled
  $newuser['status'] = 1;
  $newuser['roles'][$tblinfo->desttype] = $tblinfo->desttype;
  
  foreach ($tblinfo->fields as $destfield => $values) {
    // Ignore CCK fields - we assume another module such as content_profile will deal with them
    if (!preg_match('/^field_/', $destfield)) {
      if ($values['srcfield'] && isset($row->$values['srcfield'])) {
        $newvalue = $row->$values['srcfield'];
      } else {
        $newvalue = $values['default_value'];
      }

      // Make sense of timestamp fields
      if ($destfield == 'created' || $destfield == 'access' || $destfield == 'login') {
        $newvalue = _migrate_valid_date($newvalue);
      }
      $newuser[$destfield] = $newvalue;
    }
  }

  // Prepare the user for import. 
  $errors = migrate_destination_invoke_all('prepare_user', $newuser, $tblinfo, $row);
  
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
    $errors = migrate_destination_invoke_all('complete_user', $account, $tblinfo, $row);
    timer_stop('user completion hooks');
    
    if ($account) {
      db_query("INSERT INTO {" . $tblinfo->maptable . "}
                ($sourcekey, userid, mcsid)
                VALUES(%d, %d, %d)",
               $row->$sourcekey, $account->uid, $tblinfo->mcsid);
    }
  }
  return $errors;
}

function migrate_migrate_destination_xlat_user($uid) {
  return "user/$uid";
}
