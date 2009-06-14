<?php
/* $Id$ */

function content_profile_migrate_destination_fields_user($type) {
  // Get any CCK fields attached to content profiles
  $cp_types = content_profile_get_types();
  $fields = array();
  foreach ($cp_types as $type => $info) {
    $profile_fields = migrate_invoke_all('destination', 'fields', 'node', $type);
    foreach ($profile_fields as $key => $value) {
      if (!preg_match('/^field_/', $key)) {
        unset($profile_fields[$key]);
      } else {
        $profile_fields[$key] = $info->name . ' ' . $value;
      }
    }
    $fields = array_merge($fields, $profile_fields);
  }
  return $fields;
}

function content_profile_migrate_destination_complete_user(&$account, $tblinfo, $row) {
  // Initialize all related profile nodes
  $cp_types = content_profile_get_types();
  $cp_fields = array();
  foreach ($cp_types as $type => $info) {
    $cp_fields[$type] = content_migrate_destination_fields_node($type);
    $node[$type] = new StdClass;
    $node[$type]->type = $type;
  }
  
  // Process all CCK fields in the input as content_profile 
  foreach ($tblinfo->fields as $destfield => $values) {
    if ($values['srcfield'] && isset($row->$values['srcfield'])) {
      $newvalue = $row->$values['srcfield'];
    } else {
      $newvalue = $values['default_value'];
    }
    // Find the content profile type containing this field
    foreach ($cp_fields as $type => $info) {
      if (isset($info[$destfield])) {
        $node[$type]->$destfield = $newvalue;
      }
    }
  }

  // For each profile type, create the node attached to this user
  foreach ($cp_types as $type => $name) {
    $node[$type]->uid = $account->uid;
    $node[$type]->title = $account->name;
    $node[$type]->name = $account->name;
    // Create the node.
    // Prepare the node for import. We could have written the following loop
    // as: module_invoke_all('node_import_prepare', $node, $preview > 0);
    // but unfortunately module_invoke_all passes all argumens by value.
    foreach (module_list() as $module_name) {
      $function = $module_name . '_migrate_destination_prepare_node';
      if (function_exists($function)) {
        $errors = array_merge((array)$errors, (array)$function($node[$type], $tblinfo, $row));
      }
    }
    $node[$type] = node_submit($node[$type]);
    node_save($node[$type]);
  }
  return $errors;
}
