<?php

/**
 * We want to create default views combining messages with the content set view generating
 * the messages - basically, cloning that view and adding a join to the appropriate message
 * table. Those views may or may not be default views. In hook_views_default_views(), we
 * cannot load a default view without creating an infinite recursion. In
 * hook_views_default_views_alter(), the default views are present but not the others. So,
 * we use both hooks to catch all cases.
 */

/**
 * Implementation of hook_views_default_views()
 */
function migrate_views_default_views() {
  $views = array();
  // Go through each content set and create message views connecting source data with Drupal data
  $sql = "SELECT mcsid, view_name, sourcekey FROM {migrate_content_sets}";
  $tblresult = db_query($sql);
  while ($tblrow = db_fetch_object($tblresult)) {
    $view = view::load($tblrow->view_name, FALSE);
    if ($view) {
      _migrate_views_build_msg_view($views, $view);
    }
  }
  return $views;
}

/**
 * Implementation of hook_views_default_views_alter()
 */
function migrate_views_default_views_alter(&$views) {
  // Go through each content set and create message views connecting source data with Drupal data
  $sql = "SELECT mcsid, view_name, sourcekey FROM {migrate_content_sets}";
  $tblresult = db_query($sql);
  while ($tblrow = db_fetch_object($tblresult)) {
    $view = $views[$tblrow->view_name];
    if ($view) {
      _migrate_views_build_msg_view($views, $view);
    }
  }
}

/**
 * Clone a view, attaching it to the appropriate msg table
 *
 * @param $views
 *  Array of default views being defined. The cloned view will be added to it.
 * @param $view
 *  View object to clone.
 */
function _migrate_views_build_msg_view(&$views, $view) {
  $view = $view->clone_view();
  $view->name .= '_msg';
  $view->description = $view->name;
  $view->tag = 'migrate';
  // Do the work here:
  //  Join to the message table
  //  Support argument for message type
  $views[$view->name] = $view;
}
