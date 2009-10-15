<?php
// $Id$

/**
 * @file
 * Documentation for Migrate module's hooks.
 */

/**
 * Do one-time initialization, prior to when any migrate hook is called.
 */
function hook_migrate_init() {
  // Load in migration code.
  require_once drupal_get_path('module', 'example') . '/migrate.inc';
}

/**
 * Define possible destination "content types" which can accept incoming data.
 *
 * @return array
 *   An array mapped from the internal name of the type (used to build hook
 *   names, in particular) to the user-visible type name.
 */
function hook_migrate_types() {
  $types = array('user' => t('User'), 'role' => t('Role'));
  return $types;
}

/**
 * Expose list of possible table fields which can accept incoming data.
 *
 * @return array
 *   An array mapped from the internal field name (within Drupal) to the
 *   user-visible name.
 */
function hook_migrate_fields_$contenttype() {
  $fields = array(
    'name' => t('Role name'),
  );
  return $fields;
}

/**
 * Perform any tasks necessary when reverting a migration job.
 *
 * Establish deletion routine for object of type $contenttype and everything
 * that depends on that object.
 *
 * @param $id
 *   Unique identifier of the destination object.
 */
function hook_migrate_delete_$contenttype($id) {
  db_query('DELETE FROM {users_roles} WHERE rid = %d', $id);
  db_query('DELETE FROM {permission} WHERE rid = %d', $id);
  db_query('DELETE FROM {role} WHERE rid = %d', $id);
}

/**
 * Perform steps required to import $contenttype content.
 *
 * A common practice is to invoke 'prepare' and 'complete' hooks at appropriate
 * times, so that other modules may act upon the current object's migration.
 *
 * @param $tblinfo
 *   Meta-information about the content set.
 * @param $row
 *   Source data for one object.
 * @return
 *   Array of messages. Use migrate_message() to generate a message.
 */
function hook_migrate_import_$contenttype($tblinfo, $row) {
  $errors = array();
  $new_role = array();
  foreach ($tblinfo->fields as $destfield => $values) {
    if ($values['srcfield'] && isset($row->$values['srcfield'])) {
      $newvalue = $row->$values['srcfield'];
    }
    else {
      $newvalue = $values['default_value'];
    }
    $new_role[$destfield] = $newvalue;
  }

  // Prepare the role for import.
  $errors = migrate_destination_invoke_all('prepare_role', $new_role, $tblinfo, $row);

  $role_name = $new_role['name'];

  if ($role_name) {
    db_query("INSERT INTO {role} (name) VALUES ('%s')", $role_name);
    $sql = "SELECT rid FROM {role} WHERE name='%s'";
    $rid = db_result(db_query($sql, $role_name));
    $new_role['rid'] = $rid;
    // Call completion hooks, for any additional role-related processing
    // (such as assigning permissions).
    timer_start('role completion hooks');
    $errors += migrate_destination_invoke_all('complete_role', $new_role, $tblinfo, $row);
    timer_stop('role completion hooks');

    $sourcekey = $tblinfo->sourcekey;
    migrate_add_mapping($tblinfo->mcsid, $row->$sourcekey, $rid);
  }
  return $errors;
}

/**
 * Translate URIs from an old site to the new one.
 *
 * Requires adding RewriteRules to .htaccess. For example, if the URLs
 * for news articles had the form
 * http://example.com/issues/news/[OldID].html, use this rule:
 *
 * RewriteRule ^issues/news/([0-9]+).html$ /migrate/xlat/node/$1 [L]
 *
 * @param $newid
 *   The ID of the $contenttype object in the new site.
 * @return string
 *   The Drupal system path to the $contenttype object.
 */
function hook_migrate_xlat_$contenttype($newid) {
  return "node/$newid";
}
