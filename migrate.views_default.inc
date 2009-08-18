<?php
// $Id$

/**
 * @file
 * Modify the default views of message tables
 */

/**
 * TODO:
 * Eventually, we want to create default views combining messages with the content set view
 * generating the messages - basically, cloning that view and adding a join to the appropriate
 * message table. Those views may or may not be default views. In hook_views_default_views(), we
 * cannot load a default view without creating an infinite recursion. In
 * hook_views_default_views_alter(), the default views are present but not the others. So,
 * we use both hooks to catch all cases.
 */

/**
 * Implementation of hook_views_default_views_alter().
 */
function migrate_views_default_views_alter(&$views) {
  // Go through each content set and alter the Table Wizard's default view for its message
  // table to add an argument (level), and alter the access permission
  $sql = "SELECT mcsid, view_name, sourcekey FROM {migrate_content_sets}";
  $tblresult = db_query($sql);
  while ($tblrow = db_fetch_object($tblresult)) {
    $viewname = _migrate_message_table_name($tblrow->view_name);
    $view = $views[$viewname];
    $view->display['default']->display_options['arguments'] = array(
      'level' => array(
        'id' => 'level',
        'field' => 'level',
        'table' => $viewname,
        'relationship' => 'none',
        'default_action' => 'ignore',
      ),
    );
    $view->display['default']->display_options['access'] = array(
      'type' => 'perm',
      'perm' => MIGRATE_ACCESS_BASIC,
    );
    // Not interested in displaying these
    unset($view->display['default']->display_options['fields']['mceid']);
    unset($view->display['default']->display_options['fields']['level']);
    $view->display['default']->display_options['title'] =
      t('Migration messages for content set !view',
        array('!view' => $viewname));

    $view->display['default']->display_options['header'] =
      t('Listing of messages reported when importing this content set.');
    $views[$viewname] = $view;
  }
}
