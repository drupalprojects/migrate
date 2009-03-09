<?php
/**
 * Implementation of hook_default_view_views().
 */
/* TODO: Disabled for now, needs full reimplementation following Table Wizard refactoring
function migrate_views_default_views() {
  $views = array();

  // Go through each content set and create audit views connecting source data with Drupal data
  $sql = "SELECT mfid, importtable
          FROM {migrate_files}";
  $tblresult = db_query($sql);
  while ($tblrow = db_fetch_object($tblresult)) {
    // And, if the table is associated with any content sets (i.e., is actively being
    // used for migration), create an audit view for unimported content
    // No point unless there's at least one content set based on this table
    // TODO: Assumes a single contenttype (i.e., same table not used to create both nodes and users)
    // TODO: Supports nodes and users only for now
    $sql = "SELECT contenttype FROM {migrate_content_sets} WHERE mfid=%d";
    $contenttype = db_result(db_query($sql, $mfid));
    if ($contenttype == 'node' || $contenttype == 'user') {
      $view = new view;
      $view->name = $tablename.'_unimported';
      $view->description = t('Import table: !tablename unimported rows', array('!tablename' => $tablename));
      $view->tag = 'migrate';
      $view->view_php = '';
      $view->base_table = $tablename;
      $view->is_cacheable = FALSE;
      $view->api_version = 2;
      $view->disabled = FALSE; 
      $handler = $view->new_display('default', 'Defaults', 'default');
      $typeid = $contenttype.'id';
      $handler->override_option('relationships', array(
        $typeid => array(
          'label' => t("Migrated $contenttype"),
          'required' => 0,
          'id' => $typeid,
          'table' => $tablename,
          'field' => $typeid,
          'override' => array(
            'button' => 'Override',
          ),
          'relationship' => 'none',
        ),
        'flag_content_rel' => array(
          'label' => 'flag',
          'required' => 0,
          'flag' => $tablename,
          'user_scope' => 'any',
          'id' => 'flag_content_rel',
          'table' => $tablename,
          'field' => 'flag_content_rel',
          'relationship' => 'none',
        ),
      ));
      // Field for the exclusion flag
      $fields = array(
        'ops' => array(
          'label' => 'Flag link',
          'exclude' => 0,
          'id' => 'ops',
          'table' => 'flag_content',
          'field' => 'ops',
          'relationship' => 'flag_content_rel',
        ),
      );
      // Add our columns to the fields
      foreach ($colnames as $colname) {
        $fields[$colname] = array(
          'id' => $colname,
          'table' => $tablename,
          'field' => $colname,
        );
      }
      $handler->override_option('fields', $fields);
      if ($contenttype == 'node') {
        $dfield = 'nid';
      } else {
        $dfield = 'uid';
      }
      $handler->override_option('filters', array(
        $dfield => array(
          'operator' => 'empty',
          'value' => array(
            'value' => '',
            'min' => '',
            'max' => '',
          ),
          'group' => '0',
          'exposed' => FALSE,
          'expose' => array(
            'operator' => FALSE,
            'label' => '',
          ),
          'id' => $dfield,
          'table' => $contenttype,
          'field' => $dfield,
          'relationship' => $typeid,
        ),
        'flagged' => array(
          'operator' => '=',
          'value' => '0',
          'group' => '0',
          'exposed' => FALSE,
          'expose' => array(
            'operator' => FALSE,
            'label' => '',
          ),
          'id' => 'flagged',
          'table' => 'flag_content',
          'field' => 'flagged',
          'relationship' => 'flag_content_rel',
        ),
      ));
      $handler->override_option('access', array(
        'type' => 'perm',
        'perm' => 'basic migration tools',
      ));
      $handler->override_option('title', t('Unimported rows in !tablename', array('!tablename' => $tablename)));
      $handler->override_option('header', t('This listing shows rows from the !tablename table which have 
        not been imported into Drupal !contenttype objects, other than those that were explicitly excluded.',
        array('!tablename' => $tablename, '!contenttype' => $contenttype)));
      $handler->override_option('header_format', '1');
      $handler->override_option('header_empty', 1);
      $handler->override_option('empty', 'All rows in this import table have been migrated into Drupal.');
      $handler->override_option('empty_format', '1');
      $handler->override_option('items_per_page', 50);
      $handler->override_option('use_pager', '1');
      $handler->override_option('style_plugin', 'table');
      
      // Add our columns into the style options
      $columns = array('ops' => 'ops');
      foreach ($colnames as $colname) {
        $columns[$colname] = $colname;
      }
      $info = array(
        'ops' => array(
          'separator' => '',
        ),
      );
      foreach ($colnames as $colname) {
        $info[$colname] = array(
          'sortable' => 1,
          'separator' => '',
        );
      }
      $handler->override_option('style_options', array(
        'grouping' => '',
        'override' => 1,
        'sticky' => 1,
        'order' => 'asc',
        'columns' => $columns,
        'info' => $info,
        'default' => $pk,
      ));
      
      $handler = $view->new_display('page', 'Page', 'page_1');
      $handler->override_option('path', 'admin/content/migrate/audit/'.$tablename.'_unimported');
      $handler->override_option('menu', array(
        'type' => 'none',
        'title' => '',
        'weight' => 0,
        'name' => 'navigation',
      ));
      $handler->override_option('tab_options', array(
        'type' => 'none',
        'title' => '',
        'weight' => 0,
      ));
      $views[$view->name] = $view;
    }
  }
  
  return $views;
}
*/