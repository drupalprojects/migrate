<?php
// $Id$

/**
 * @file
 */

/**
 * Form definition for dashboard page
 */
function migrate_dashboard($form, $form_state) {
  $form = array();
  $form['description'] = array(
    '#prefix' => '<div>',
    '#markup' => "This page lists implemented migrations.",
    '#suffix' => '</div>',
  );

  $header = array(
    array('data' => t('Status')),
    array('data' => t('Migration')),
    array('data' => t('Total rows')),
    array('data' => t('Imported')),
    array('data' => t('Unimported')),
    array('data' => t('Last imported')),
    );

  $migrations = migrate_migrations();

  $rows = array();
  foreach ($migrations as $migration) {
    $row = array();
    $total = $migration->source->count();
    $imported = $migration->importedCount();
    $unimported = $total - $imported;
    $status = $migration->getStatus();
    switch ($status) {
      case Migration::STATUS_IDLE:
        $status = t('Idle');
        break;
      case Migration::STATUS_IMPORTING:
        $status = t('Importing');
        break;
      case Migration::STATUS_ROLLING_BACK:
        $status = t('Rolling back');
        break;
      default:
        $status = t('Unknown');
        break;
    }
    $table[] = array($migration->machineName, $total, $imported, $total-$imported, $status,
        $migration->getLastImported());

    $row['status'] = $status;
    $row['machinename'] =
      l($migration->machineName, 'admin/content/migrate/migration/' . $migration->machineName);
    $row['importrows'] = $total;
    $row['imported'] = $imported;
    $row['unimported'] = $unimported;
    $row['lastimported'] = $migration->getLastImported();
    $rows[] = $row;
  }

  $form['dashboard'] = array(
    '#prefix' => '<div>',
    '#markup' => theme('table', array('header' => $header, 'rows' => $rows)),
    '#suffix' => '</div>',
  );
  return $form;
}

/**
 * Theme function for dashboard page
 */
function theme_migrate_dashboard($form) {
  $output = drupal_render($form['machinename']);
  if (isset($form['data']) && is_array($form['data'])) {
    foreach (element_children($form['data']) as $rownum) {
      $row = array();
      foreach (element_children($form['data'][$rownum]) as $colname) {
        $row[] = drupal_render($form['data'][$rownum][$colname]);
      }
      // Highlight any process currently running
/*      if ($form['data'][$rownum]['status']['#value']) {
        $rows[] = array('data' => $row, 'class' => 'migrate-running');
      }
      else {*/
        $rows[] = $row;
      //}
    }
  }

  $header = $form['header']['#value'];

  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('class' => 'migrate-dashboard'), 'empty' => t('No data in the table.')));
  $output .= drupal_render($form);

  return $output;
}

/**
 * Menu callback function for migration view page.
 */
function migrate_migration_info($form, $form_state, $machine_name) {
  $row = migrate_content_set_load($mscid);
  $desttype = $row->desttype;
  $view_name = $row->view_name;
  $view_args = $row->view_args;
  $sourcekey = $row->sourcekey;
  $machine_name = $row->machine_name;
  $description = $row->description;
  $contenttype = $row->contenttype;
  $desttype = $row->desttype;
  $weight = $row->weight;
  $multiple_separator = $row->multiple_separator;

  drupal_set_title($description);

  // Fetch information on available destinations
  $desttypes = migrate_invoke_all('types');
  $destfields = migrate_invoke_all("fields_$contenttype", $desttype);

  $form['mcsid'] = array(
    '#type' => 'value',
    '#value' => $mcsid,
  );

  $form['machine_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Content set name'),
    '#size' => 24,
    '#maxlength' => 50,
    '#default_value' => $machine_name,
    '#description' => t('This is the unique name of the content set. It must contain
      only alphanumeric characters and underscores; it is used to identify the
      content set internally and to generate map and message tables related to
      the content set.'),
  );

  $form['description'] = array(
    '#type' => 'textfield',
    '#title' => t('Description of the content set'),
    '#default_value' => $description,
  );
  $form['show_view_name'] = array(
    '#prefix' => '<div>',
    '#value' => t('<strong>Source view:</strong> ') . l($view_name, 'admin/build/views/edit/' . $view_name),
    '#suffix' => '</div>',
  );
  $form['view_args'] = array(
    '#type' => 'textfield',
    '#title' => t('View arguments'),
    '#description' => t('Arguments to apply to the view when processing,
      separated with a / as though they were a URL path.'),
    '#default_value' => $view_args,
  );

  $form['view_name'] = array(
    '#type' => 'value',
    '#value' => $view_name,
  );
  if ($desttype) {
    $destination = $desttypes["$contenttype/$desttype"];
  }
  else {
    $destination = $desttypes[$contenttype];
  }
  $form['show_contenttype'] = array(
    '#prefix' => '<div>',
    '#value' => t('<strong>Destination:</strong> ') . $destination,
    '#suffix' => '</div>',
  );
  $form['contenttype'] = array(
    '#type' => 'value',
    '#value' => $contenttype,
  );
  $form['desttype'] = array(
    '#type' => 'value',
    '#value' => $desttype,
  );

  $form['weight'] = array(
    '#type' => 'textfield',
    '#title' => t('Weight'),
    '#description' => t('The order in which content sets will be processed and displayed.'),
    '#default_value' => $weight,
  );

  $form['multiple_separator'] = array(
    '#type' => 'textfield',
    '#title' => t('Separator'),
    '#description' => t('Separator for fields potentially holding multiple values, e.g. taxonomy terms.'),
    '#size' => 2,
    '#maxlength' => 8,
    '#default_value' => $multiple_separator,
  );

  $form['header'] = array(
    '#type' => 'value',
    '#value' => array(
      array('data' => t('Source field')),
      array('data' => t('Default value')),
      array('data' => t('Destination field')),
    ),
  );

  $view = views_get_view($view_name);
  if (!$view) {
    drupal_set_message(t('View !view does not exist - either (re)create a view with
      this name, or delete this content set.', array('!view' => $view_name)));
  }
  else {
    // Need to fill in the query, to find out the aliases that will be returned by the
    // query
    if ($view_args) {
      $view->set_arguments(explode('/', $view_args));
    }
    $view->build();
    $fields = $view->get_items('field');
    $srcoptions = array();
    foreach ($view->query->fields as $fieldalias => $field) {
      $fieldname = $field['field'];
      $fieldtable = $field['table'];
      // The field name can be ambiguous (e.g., two map tables in the view), so
      // we can't just do $fields[$fieldname] - we need to iterate and match the
      // table as well
      foreach ($fields as $viewfieldname => $viewfield) {
        if ($viewfield['field'] == $fieldname && $viewfield['table'] == $fieldtable) {
          $srcoptions[$fieldalias] = $viewfield['label'];
          break;
        }
      }
      if (!isset($srcoptions[$fieldalias])) {
        $srcoptions[$fieldalias] = $fieldtable . '.' . $fieldname;
      }
    }

    $form['sourcekey'] = array(
      '#type' => 'select',
      '#options' => $srcoptions,
      '#default_value' => $sourcekey,
      '#title' => t('Primary key of source view'),
    );

    $mappings = array();
    $defaults = array();

    $srcoptions = array_merge(array('' => t('<none>')), $srcoptions);
    foreach ($destfields as $destfield => $destname) {
      $matches = array();
      if (preg_match('/^\[([^\]]*)\]$/', $destfield, $matches)) {
        $primary_key = $matches[1];
        $destfield = $primary_key;
        $destfields[$primary_key] = $destname;
        unset($destfields[$destfield]);
      }
      else {
        $primary_key = '';
      }
      $sql = "SELECT *
              FROM {migrate_content_mappings}
              WHERE mcsid=%d AND destfield='%s'";
      $result = db_query($sql, $mcsid, $destfield);
      $row = db_fetch_object($result);
      $cols[] = $destfield;
      $form['srcfield'][$destfield] = array(
        '#type' => 'select',
        '#options' => $srcoptions,
        '#default_value' => isset($row->srcfield) ? $row->srcfield : '',
      );
      $form['default_value'][$destfield] = array(
        '#type' => 'textfield',
        '#default_value' => isset($row->default_value) ? $row->default_value : '',
        '#size' => 25,
        '#maxlength' => 255,
      );
      $form['destfield'][$destfield] = array('#value' => $destname);
    }

    $form['cols'] = array(
      '#type' => 'value',
      '#value' => $cols,
    );

    $form['primary_key'] = array(
      '#type' => 'value',
      '#value' => $primary_key,
    );

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Submit changes'),
    );
  }
  $form['delete'] = array(
    '#type' => 'submit',
    '#value' => t('Delete'),
  );
  $form['#tree'] = TRUE;
  return $form;
}

/**
 * Theme function for content set edit page.
 */
function theme_migrate_content_set_mappings($form) {
  $output = drupal_render($form['machine_name']);
  $output .= drupal_render($form['description']);
  $output .= drupal_render($form['show_view_name']);
  $output .= drupal_render($form['view_args']);
  $output .= drupal_render($form['show_contenttype']);
  $output .= drupal_render($form['desttype']);
  $output .= drupal_render($form['sourcekey']);
  $output .= drupal_render($form['weight']);
  $output .= drupal_render($form['multiple_separator']);

  if (isset($form['destfield']) && is_array($form['destfield'])) {
    foreach (element_children($form['destfield']) as $destfield) {
      $row = array();
      $row[] = drupal_render($form['srcfield'][$destfield]);
      $row[] = drupal_render($form['default_value'][$destfield]);
      $row[] = drupal_render($form['destfield'][$destfield]);
      $rows[] = $row;
    }
  }

  $header = $form['header']['#value'];
  if (!$rows) {
    $rows[] = array(array('data' => t('No data in the table.'), 'colspan' => count($header)));
  }
  $output .= t('Map fields in the Source recordset to properties in a Drupal object. More complex fields such as multiple value elements need to be handled in code using the prepare hook.');
  $output .= theme('table', array('header' => $header, 'rows' => $rows));
  $output .= drupal_render($form['submit']);
  $output .= drupal_render($form);

  // Support dynamic visualization of already-picked fields
  //drupal_add_js(drupal_get_path('module', 'migrate') .'/migrate.js');

  return $output;
}


/**
 * Form definition for settings page.
 */
function _migrate_settings_form($form_state) {
  $max_execution_time = ini_get('max_execution_time');
  $max_input_time = ini_get('max_input_time');
  $memory_limit = _migrate_memory_limit();
  $description = '';
  if ($max_execution_time < 90 || $max_input_time < 90) {
    $description .= t(
      '<p>Each batch of a migration performed interactively will last
        a few seconds less than the less of <em>max_execution_time</em>
        and <em>max_input_time</em>. It is recommended that you set these
        to a higher value (say, 240) in .htaccess.</p>'
    );
  }
  if ($memory_limit < 128*1024*1024) {
    $description .= t(
      '<p>Large migration operations can take substantial memory, particularly
        if there is cached information growing with each iteration. It is
        recommended that you set <em>memory_limit</em> higher (at least
        256M if you can).</p>'
    );
  }
  $description .= t('<p>Current PHP configuration options:
    <table>
      <tr><td>max_execution_time</td><td>!max_execution_time</td></tr>
      <tr><td>max_input_time</td><td>!max_input_time</td></tr>
      <tr><td>memory_limit</td><td>!memory_limit</td></tr>
    </table>
    </p>',
    array('!max_execution_time' => $max_execution_time,
          '!max_input_time' => $max_input_time,
          '!memory_limit' => ini_get('memory_limit')
         )
  );
  $form['description'] = array(
    '#prefix' => '<div>',
    '#markup' => $description,
    '#suffix' => '</div>',
  );
  $form['display_timers'] = array(
    '#type' => 'checkbox',
    '#title' => t('Display timers when processing'),
    '#description' => t('To diagnose performance bottlenecks, turn this toggle
      on - at the completion of a processing round, cumulative times of
      tasks will be displayed.'),
    '#default_value' => variable_get('migrate_display_timers', 0),
  );

  $form['hide_help_message'] = array(
    '#type' => 'checkbox',
    '#title' => t('Ignore missing advanced help module'),
    '#description' => t('Migrate uses the Advanced Help module to provide help text; if this module is not present Migrate will complain, unless this setting is checked.'),
    '#default_value' => variable_get('migrate_hide_help_message', FALSE),
  );

  $form['cache_counts'] = array(
    '#type' => 'checkbox',
    '#title' => t('Cache content set counts'),
    '#description' => t('With large and/or complex content sets, getting the <strong>Total
      Rows</strong> value on the dashboard page can be time-consuming. Enable caching to
      store the last known count in the database.'),
    '#default_value' => variable_get('migrate_cache_counts', 0),
  );

  if (variable_get('migrate_cache_counts', 0)) {
    $form['refresh_counts'] = array(
      '#type' => 'checkbox',
      '#title' => t('Refresh cached content set counts'),
      '#description' => t('Update the cached content set counts to reflect their current values.'),
      '#default_value' => 0,
    );
  }
  $form['integrations'] = array('#tree' => TRUE);
  $form['integrations']['description'] = array(
    '#prefix' => '<hr /><div>',
    '#markup' => t('Modules implementing the Migrate API are listed below. Most modules
                   other than the Migrate module itself simply support themselves, but
                   some (notably <a href="http://drupal.org/project/migrate_extras">Migrate Extras</a>)
                   may provide support on behalf of other modules. Support can be
                   disabled by unchecking the box for a particular line.'),
    '#suffix' => '</div>',
  );

  foreach (migrate_get_module_apis() as $module => $info) {
    $form['integrations'][$module] = array(
      '#type' => 'fieldset',
      '#title' => t('Migration support implemented in the @module module',
                    array('@module' => $module)),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    foreach ($info['integration modules'] as $intmod => $intmod_details) {
       $form['integrations'][$module][$intmod] = array(
         '#type' => 'checkbox',
         '#title' => t($intmod_details['description']),
         '#default_value' => $intmod_details['status'],
       );
    }
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Submit"),
  );
  return $form;
}

/**
 * Theme function for settings form.
 */
function theme_migrate_settings($form) {
  $output = '';
  $output .= drupal_render($form['description']);
  $output .= drupal_render($form['display_timers']);
  $output .= drupal_render($form['hide_help_message']);
  $output .= drupal_render($form['cache_counts']);
  $output .= drupal_render($form['refresh_counts']);
  $output .= drupal_render($form['integrations']);
  $output .= drupal_render($form);

  return $output;
}

/*
 * FAPI submit handler().
 */
function _migrate_settings_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  variable_set('migrate_display_timers', $values['display_timers']);
  variable_set('migrate_hide_help_message', $values['hide_help_message']);
  $original_cache = variable_get('migrate_cache_counts', 0);
  variable_set('migrate_cache_counts', $values['cache_counts']);

  variable_set('migrate_integration_settings', $values['integrations']);
  if (isset($values['refresh_counts']) || ($values['cache_counts'] && !$original_cache)) {
    $sql = "SELECT mcsid,view_name,view_args FROM {migrate_content_sets}";
    $result = db_query($sql);
    foreach ($result as $row) {
      $rowcount = _migrate_get_view_count($row->view_name, $row->view_args);
      db_update('migrate_content_sets')
        ->fields(array('rowcount' => $rowcount))
        ->condition('mscid', $row->mscid)
        ->execute();
    }
    if ($values['refresh_counts']) {
      drupal_set_message(t('Content set counts refreshed'));
    }
    else {
      drupal_set_message(t('Content set counts saved'));
    }
  }
  drupal_set_message(t('Settings saved'));
}

/**
 * Menu callback function.
 */
function migrate_export_content_set($form_state, $mcsid) {
  $row = (array) migrate_content_set_load($mcsid);
  drupal_set_title($row['description']);
  // TODO: Generate Table Wizard calls?
  $code = '  $content_set = new stdClass;' . "\n";
  unset($row['mcsid']);
  foreach ($row as $field => $value) {
    // Yes, we're potentially quoting integer values - it all works out
    $code .= '  $content_set->' . $field . "= '" . $value . "';\n";
  }
  $code .= '  migrate_save_content_set($content_set);' . "\n";
  $code .= '  $mcsid = $content_set->mcsid;' . "\n\n";

  $sql = "SELECT * FROM {migrate_content_mappings} WHERE mcsid=%d";
  $result = db_query($sql, $mcsid);
  while ($row = db_fetch_array($result)) {
    unset($row['mcmid']);
    unset($row['mcsid']);
    $code .= '  $mapping = new stdClass' . "\n";
    $code .= '  $mapping->mcsid = $mcsid' . "\n";
    foreach ($row as $field => $value) {
      $code .= '  $mapping->' . $field . "= '" . $value . "';\n";
    }
    $code .= '  migrate_save_content_mapping($mapping);' . "\n\n";
  }
  $lines = substr_count($code, "\n");

  $form['description'] = array(
    '#prefix' => '<div>',
    '#value' => t('Copy this code to the the clipboard and paste it into an
      install or update hook. This enables you to maintain content sets
      programmatically, and to put their definitions under source control.'),
    '#suffix' => '</div>',
  );

  $form['export'] = array(
    '#title' => t('Export data'),
    '#type' => 'textarea',
    '#value' => $code,
    '#rows' => $lines,
  );
  return $form;
}
