<?php
// $Id$
define('MIGRATE_COLS_ALL', 1);
define('MIGRATE_COLS_EMPTY', 2);
define('MIGRATE_COLS_NONEMPTY', 3);


/**
 * @file
 */

/**
 * Menu callback for migrate front page
 */
function migrate_front() {
  migrate_check_advanced_help();

  module_load_include('inc', 'system', 'system.admin');
  return system_admin_menu_block_page();
}

/**
 * Menu callback for dashboard page
 */
function migrate_dashboard() {
  return drupal_get_form('_migrate_dashboard_form');
}

/**
 * Form definition for dashboard page
 */
function _migrate_dashboard_form($form_state) {
  $header = array(
    array('data' => t('Clear')),
    array('data' => t('Import')),
    array('data' => t('Scan')),
    array('data' => t('Content Set')),
    array('data' => t('Source')),
    array('data' => t('Rows in Set')),
    array('data' => t('Imported')),
    array('data' => t('Unimported')),
    array('data' => t('Last imported')),
    );
  $form['header'] = array('#type' => 'value', '#value' => $header);
  $sql = "SELECT *
          FROM {migrate_content_sets}
          ORDER BY weight, contenttype, view_name, view_args";
  $result = db_query($sql);

  $clearing = array();
  $clearingenable = array();
  $importing = array();
  $importingenable = array();
  $scanning = array();
  $scanningenable = array();
  $rownum = 0;
  while ($row = db_fetch_object($result)) {
    $status = '';
    if ($row->mcsid) {
      $view = views_get_view($row->view_name);
      if (!$view) {
        drupal_set_message(t('View !view does not exist - either (re)create this view, or
          remove the content set using it.', array('!view' => $row->view_name)));
        continue;
      }
      $clearing[$row->mcsid] = '';
      $importing[$row->mcsid] = '';
      $scanning[$row->mcsid] = '';
      $status = t('N/A');
      $maptable = migrate_map_table_name($row->mcsid);
      $sourcekey = $row->sourcekey;
      if ($row->clearing) {
        $clearingenable[] = $row->mcsid;
      }
      if ($row->importing) {
        $importingenable[] = $row->mcsid;
      }
      if ($row->scanning) {
        $scanningenable[] = $row->mcsid;
      }
      $imported = db_result(db_query('SELECT COUNT(*) FROM {' . $maptable . '}'));

      // If not caching counts, override the saved count with a fresh count
      if (!variable_get('migrate_cache_counts', 0)) {
        $row->rowcount = _migrate_get_view_count($view, $row->view_args);
      }

      $unimported = $row->rowcount - $imported;

      $msgtablename = migrate_message_table_name($row->mcsid);
      if ($unimported > 0) {
        $messages = '';
        /* Restore when views integration is fully implemented
        $unimported = l($unimported, 'admin/content/migrate/audit/'.$row->view_name.'_unimported');*/
        $numerrors = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                         FROM {" . $msgtablename . "}
                                         WHERE level=%d",
                                        MIGRATE_MESSAGE_ERROR));
        if ($numerrors > 0) {
          $messages .= l(format_plural($numerrors, '1&nbsp;error', '@count&nbsp;errors'),
            "admin/content/tw/view/$msgtablename/" . MIGRATE_MESSAGE_ERROR,
            array('html' => TRUE))
            . '<br />';
        }
        $numwarnings = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                           FROM {" . $msgtablename . "}
                                           WHERE level=%d",
                                          MIGRATE_MESSAGE_WARNING));
        if ($numwarnings > 0) {
          $messages .= l(format_plural($numwarnings, '1&nbsp;warning', '@count&nbsp;warnings'),
            "admin/content/tw/view/$msgtablename/" . MIGRATE_MESSAGE_WARNING,
            array('html' => TRUE))
            . '<br />';
        }
        $numnotices = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                           FROM {" . $msgtablename . "}
                                           WHERE level=%d",
                                          MIGRATE_MESSAGE_NOTICE));
        if ($numnotices > 0) {
          $messages .= l(format_plural($numnotices, '1&nbsp;notice', '@count&nbsp;notices'),
            "admin/content/tw/view/$msgtablename/" . MIGRATE_MESSAGE_NOTICE,
            array('html' => TRUE))
            . '<br />';
        }
        if ($messages) {
          $unimported = $messages . " $unimported&nbsp;total";
        }
      }
      if ($imported > 0) {
        $numinformational = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                           FROM {" . $msgtablename . "}
                                           WHERE level=%d",
                                          MIGRATE_MESSAGE_INFORMATIONAL));
        if ($numinformational > 0) {
          $imported .= '<br />' .
            l(format_plural($numinformational, '1 informational message', '@count informational messages'),
            "admin/content/tw/view/$msgtablename/" . MIGRATE_MESSAGE_INFORMATIONAL,
            array('html' => TRUE))
            . '<br />';
        }
      }
    }
    else {
      $imported = '';
      $unimported = '';
    }

    $form['data'][$rownum]['clearing'] = array('#value' => check_plain($row->clearing));
    $form['data'][$rownum]['importing'] = array('#value' => check_plain($row->importing));
    $form['data'][$rownum]['scanning'] = array('#value' => check_plain($row->scanning));
    $form['data'][$rownum]['description'] = array('#value' =>
      l($row->description, 'admin/content/migrate/content_sets/' . $row->mcsid));
    $form['data'][$rownum]['view_name'] = array('#value' =>
      l($row->view_name, 'admin/build/views/edit/'. $row->view_name));
    $form['data'][$rownum]['importrows'] = array('#value' => check_plain($row->rowcount));
    $form['data'][$rownum]['imported'] = array('#value' => $imported);
    $form['data'][$rownum]['unimported'] = array('#value' => $unimported);
    $form['data'][$rownum]['lastimported'] = array('#value' => $row->lastimported);
    $form['data'][$rownum]['mcsid'] = array('#value' => check_plain($row->mcsid));
    $form['data'][$rownum]['semaphore'] = array('#value' => $row->semaphore);
    $rownum++;
  }

  $form['clearing'] = array(
    '#type' => 'checkboxes',
    '#options' => $clearing,
    '#default_value' => $clearingenable,
  );
  $form['importing'] = array(
    '#type' => 'checkboxes',
    '#options' => $importing,
    '#default_value' => $importingenable,
  );
  $form['scanning'] = array(
    '#type' => 'checkboxes',
    '#options' => $scanning,
    '#default_value' => $scanningenable,
  );
  if (user_access(MIGRATE_ACCESS_ADVANCED)) {
    $form['interactive'] = array(
      '#type' => 'fieldset',
      '#title' => t('Interactive mode'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
      '#description' => t('While large migration tasks are best run in the non interactively (via drush or cron),
        you may choose here to begin tasks interactively; either because they are relatively small,
        or to test subsets of the migration.'),
    );
    $form['interactive']['donow'] = array(
      '#type' => 'checkbox',
      '#default_value' => 1,
      '#title' => t('Enable'),
      '#description' => t('If enabled, processing of selected processes begins immediately
        upon clicking Submit, with any unfinished processing continued in the background.
        If disabled, processing will be done entirely in the background beginning with the next cron.'),
    );
    $form['interactive']['limit'] = array(
      '#type' => 'textfield',
      '#title' => t('Sample size'),
      '#size' => 4,
      '#description' => t('Number of records to process in the current interactive run. Leave
        blank to process all records that can be completed within the PHP max_execution_time.'),
      '#default_value' => variable_get('migrate_limit', ''),
    );
    $form['interactive']['idlist'] = array(
      '#type' => 'textfield',
      '#title' => t('Source IDs'),
      '#size' => 30,
      '#description' => t('Enter a comma-separated list of IDs from the incoming content set, to
        process only those records in the current interactive run.'),
    );
    $form['clearsemaphores'] = array(
      '#type' => 'checkbox',
      '#title' => t('Unstick migration process'),
      '#description' => t('An error during processing can leave a migration semaphore enabled,
        preventing further processing. When this happens, you will see the message that a content
        set "has an operation already in progress" when you are sure there is no process currently running
        (even in cron or drush). Check this box to clear the semaphores when this happens.'),
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t("Submit"),
    );
  }
  return $form;
}

/**
 * Theme function for dashboard page
 */
function theme_migrate_dashboard($form) {
  $output = drupal_render($form['description']);
  if (isset($form['data']) && is_array($form['data'])) {
    foreach (element_children($form['data']) as $rownum) {
      $row = array();
      foreach (element_children($form['data'][$rownum]) as $colname) {
        if ($colname == 'clearing' || $colname == 'importing' || $colname == 'scanning') {
          $row[] = drupal_render($form[$colname][$form['data'][$rownum]['mcsid']['#value']]);
          // Throw out the column contents
          drupal_render($form['data'][$rownum][$colname]);
          drupal_render($form['data'][$rownum]['mcsid']);
        }
        elseif ($colname == 'semaphore') {
          drupal_render($form['data'][$rownum][$colname]);
        }
        else {
          $row[] = drupal_render($form['data'][$rownum][$colname]);
        }
      }
      // Highlight any process currently running
      if ($form['data'][$rownum]['semaphore']['#value']) {
        $rows[] = array('data' => $row, 'class' => 'migrate-running');
      }
      else {
        $rows[] = $row;
      }
    }
  }

  $header = $form['header']['#value'];
  if (empty($rows)) {
    $rows[] = array(array('data' => t('No data in the table.'), 'colspan' => count($header)));
  }

  $output .= theme('table', $header, $rows, array('class' => 'migrate-dashboard'));
  $output .= drupal_render($form);

  return $output;
}

/*
 * Implementation of hook_submit().
 */
function _migrate_dashboard_form_submit($form, &$form_state) {
  $started = time();
  foreach ($form_state['values']['importing'] as $mcsid => $value) {
    if ($value) {
      $importing = TRUE;
    }
    else {
      $importing = FALSE;
    }
    if ($form_state['values']['clearing'][$mcsid]) {
      $clearing = TRUE;
    }
    else {
      $clearing = FALSE;
    }
    if ($form_state['values']['scanning'][$mcsid]) {
      $scanning = TRUE;
    }
    else {
      $scanning = FALSE;
    }
    db_query("UPDATE {migrate_content_sets}
              SET clearing=%d, importing=%d, scanning=%d
              WHERE mcsid=%d",
             $clearing, $importing, $scanning, $mcsid);
  }
  if ($form_state['values']['clearsemaphores']) {
    db_query("UPDATE {migrate_content_sets} SET semaphore=0");
  }
  if ($form_state['values']['donow']) {
    variable_set('migrate_limit', $form_state['values']['limit']);
    $batch = array(
      'operations' => array(
        array('migrate_content_process_all_batch', array($form_state['values']['limit'],
          trim($form_state['values']['idlist'])))),
      'title' => t('Migration processing'),
      'file' => drupal_get_path('module', 'migrate') . '/migrate_pages.inc',
      'init_message' => t('Starting migration process'),
      'progress_message' => t(''),
      'error_message' => t('An error occurred. Some or all of the migrate processing has failed.'),
      'finished' => '_migrate_content_process_finish',
    );

    batch_set($batch);
  }
}

/**
 * Batch API finished callback - report results
 *
 * @param $success
 *  Ignored
 * @param $results
 *  List of results from batch processing
 * @param $operations
 *  Ignored
 */
function _migrate_content_process_finish($success, $results, $operations) {
  foreach ($results as $result) {
    drupal_set_message($result);
  }
}

/**
 * Menu callback for content sets page
 */
function migrate_content_sets() {
  return (drupal_get_form('_migrate_content_set_form'));
}

/**
 * Form definition for content sets page
 */
function _migrate_content_set_form($form_state) {
  // Fetch information on available destinations
  $desttypes = migrate_invoke_all('types');

  $header = array(
    array('data' => t('Description'), 'field' => 'description'),
    array('data' => t('Destination'), 'field' => 'contenttype'),
    array('data' => t('Source view'), 'field' => 'view_name'),
    array('data' => t('View args'), 'field' => 'view_args'),
    array('data' => t('Weight'), 'field' => 'weight', 'sort' => 'asc'),
  );

  $sql = "SELECT *
          FROM {migrate_content_sets} ";
  $sql .= tablesort_sql($header);
  $result = db_query($sql);
  while ($row = db_fetch_object($result)) {
    if ($row->desttype) {
      $destination = $desttypes[$row->contenttype . '/' . $row->desttype];
    }
    else {
      $destination = $desttypes[$row->contenttype];
    }
    $rows[] = array('data' =>
      array(
        l($row->description, 'admin/content/migrate/content_sets/' . $row->mcsid,
          array('html' => TRUE)),
        $destination,
        l($row->view_name, 'admin/build/views/edit/' . $row->view_name),
        $row->view_args,
        $row->weight,
      ),
      'class' => "migrate-content-sets-tr",
    );
  }

  if (!isset($rows)) {
    $rows[] = array(array('data' => t('No content sets have been defined'), 'colspan' => count($header)));
  }

  $form['setlist'] = array(
    '#value' => theme('table', $header, $rows, array('id' => 'migrate-content-sets')),
  );
  $form['addset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Add a content set'),
    '#collapsible' => TRUE,
  );

  $form['addset']['description'] = array(
    '#type' => 'textfield',
    '#title' => t('Description of the content set'),
    '#description' => t('A friendly phrase only used for display.'),
    '#required' => TRUE,
  );

  $form['addset']['contenttype'] = array(
    '#type' => 'select',
    '#title' => t('Destination'),
    '#options' => $desttypes,
    '#description' => t('The type of Drupal content which will store the incoming records.'),
  );

  $views = views_get_all_views();
  foreach ($views as $name => $view) {
    if ($view->tag) {
      $options[$name] = $view->tag . ': ' . $name;
    }
    else {
      $options[$name] = $name;
    }
    if ($view->description) {
      if (drupal_strlen($view->description) > 60) {
        $view->description = drupal_substr($view->description, 0, 57) . '...';
      }
      $options[$name] .= " ($view->description)";
    }
  }
  asort($options);
  $form['addset']['view_name'] = array(
    '#type' => 'select',
    '#title' => t('Source view from which to import content'),
    '#options' => $options,
    '#description' => t('This View defines the records which are to be migrated.'),
  );

  $form['addset']['view_args'] = array(
    '#type' => 'textfield',
    '#title' => t('View arguments'),
    '#description' => t('Arguments to apply to the view when processing,
      separated with a / as though they were a URL path.'),
    '#maxlength' => 255,
    '#size' => 30,
  );
  $form['addset']['weight'] = array(
    '#type' => 'textfield',
    '#title' => t('Weight'),
    '#required' => TRUE,
    '#default_value' => 0,
    '#description' => t('The order in which content sets will be processed and displayed.'),
  );
  $form['addset']['add'] = array(
    '#type' => 'submit',
    '#value' => t('Add'),
  );
  return $form;
}

/**
 * Common validation function for content sets
 */
function _migrate_content_set_validate($form_state) {
  $weight = trim($form_state['values']['weight']);
  if (!is_numeric($weight) || (((int)$weight) != $weight)) {
    form_set_error('weight', t('Weight must be an integer value (positive or negative)'));
  }
  $view_name = $form_state['values']['view_name'];
  $view_args = trim($form_state['values']['view_args']);
  if (isset($form_state['values']['mcsid'])) {
    $sql = "SELECT mcsid FROM {migrate_content_sets}
            WHERE view_name='%s' AND view_args='%s' AND mcsid<>%d";
    $mcsid = db_result(db_query($sql, $view_name, $view_args, $form_state['values']['mcsid']));
  }
  else {
    $sql = "SELECT mcsid FROM {migrate_content_sets}
            WHERE view_name='%s' AND view_args='%s'";
    $mcsid = db_result(db_query($sql, $view_name, $view_args));
  }
  if ($mcsid) {
    if ($view_args) {
      form_set_error('view_name', t('There is already a content set with this
        combination of source view and view arguments.'));
    }
    else {
      form_set_error('view_name', t('There is already a content set with this
        source view and no view arguments.'));
    }
  }
}

/**
 * Implementation of hook_validate().
 */
function _migrate_content_set_form_validate($form, $form_state) {
  _migrate_content_set_validate($form_state);
}

/**
 * Implementation of hook_submit().
 */
function _migrate_content_set_form_submit($form, &$form_state) {
  $content_set['view_name'] = $form_state['values']['view_name'];
  $content_set['view_args'] = trim($form_state['values']['view_args']);
  $content_set['description'] = $form_state['values']['description'];
  $destination = explode('/', $form_state['values']['contenttype']);
  $content_set['contenttype'] = $destination[0];
  if (isset($destination[1])) {
    $content_set['desttype'] = $destination[1];
  }
  $content_set['weight'] = $form_state['values']['weight'];
  $mcsid = migrate_save_content_set($content_set);
  // Go straight to the mapping form
  $form_state['redirect'] = "admin/content/migrate/content_sets/$mcsid";
}

/**
 * Menu callback function for content set edit page.
 */
function migrate_content_set_mappings($form_state, $mcsid) {
  $row = db_fetch_object(db_query("SELECT * FROM {migrate_content_sets}
                                   WHERE mcsid=%d",
                                  $mcsid));
  $desttype = $row->desttype;
  $view_name = $row->view_name;
  $view_args = $row->view_args;
  $sourcekey = $row->sourcekey;
  $description = $row->description;
  $contenttype = $row->contenttype;
  $desttype = $row->desttype;
  $weight = $row->weight;
  $multiple_separator = $row->multiple_separator;

  // Fetch information on available destinations
  $desttypes = migrate_invoke_all('types');
  $destfields = migrate_invoke_all("fields_$contenttype", $desttype);

  $form['mcsid'] = array(
    '#type' => 'value',
    '#value' => $mcsid,
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
    $view->build();
    $fields = $view->get_items('field');
    $srcoptions = array();
    foreach ($view->query->fields as $fieldalias => $field) {
      $fieldname = $field['field'];
      $fieldtable = $field['table'];
      // The field name can be ambiguous (e.g., two map tables in the view), so
      // we can't just do $fields[$fieldname] - we need to iterate and match the
      // table as weel
      foreach ($fields as $viewfieldname => $viewfield) {
        if ($viewfield['field'] == $fieldname && $viewfield['table'] == $fieldtable) {
          $srcoptions[$fieldalias] = $viewfield['label'];
          break;
        }
      }
      if (!isset($srcoptions[$fieldalias])) {
        $srcoptions[$fieldalias] = $fieldname;
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
  $output = drupal_render($form['description']);
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
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($form['submit']);
  $output .= drupal_render($form);

  // Support dynamic visualization of already-picked fields
  drupal_add_js(drupal_get_path('module', 'migrate') .'/migrate.js');

  return $output;
}

/**
 * Implementation of hook_validate().
 */
function migrate_content_set_mappings_validate($form, $form_state) {
  _migrate_content_set_validate($form_state);
}

/**
 * Implementation of hook_submit().
 */
function migrate_content_set_mappings_submit($form, &$form_state) {
  $mcsid = $form_state['values']['mcsid'];
  if ($form_state['clicked_button']['#parents'][0] == 'delete') {
    migrate_delete_content_set($mcsid);
    drupal_set_message(t('Content set deleted'));
    $form_state['redirect'] = 'admin/content/migrate/content_sets';
  }
  else {
    // Get list of current mappings, so we know if any need deletion
    $sql = "SELECT mcmid,destfield FROM {migrate_content_mappings} WHERE mcsid=%d";
    $result = db_query($sql, $mcsid);
    $prevdests = array();
    while ($row = db_fetch_object($result)) {
      $prevdests[$row->destfield] = $row->mcmid;
    }
    $sourcekey = $form_state['values']['sourcekey'];
    foreach ($form_state['values']['cols'] as $key => $destfield) {
      $mapping = new stdClass;
      $mapping->mcsid = $mcsid;
      $mapping->srcfield = $form_state['values']['srcfield'][$destfield];
      $mapping->destfield = $destfield;
      $mapping->default_value = $form_state['values']['default_value'][$destfield];
      $mapping->mcmid = db_result(db_query(
        "SELECT mcmid
         FROM {migrate_content_mappings}
         WHERE mcsid=%d AND destfield='%s'",
        $mcsid, $destfield));
      migrate_save_content_mapping($mapping);
      // This mapping is currently valid, so remove from the deletion list
      unset($prevdests[$mapping->destfield]);
    }
    // Delete any mappings we didn't save
    foreach ($prevdests as $mcmid) {
      migrate_delete_content_mapping($mcmid);
    }
    migrate_save_content_set($form_state['values']);
    drupal_set_message('Changes saved');
  }
}

/**
 * Menu callback for tools page.
 */
function migrate_tools() {
  return drupal_get_form('_migrate_tools_form');
}

/**
 * Form definition for tools page
 */
function _migrate_tools_form($form_state) {
  $header = array(
    array('data' => t('Clearing')),
    array('data' => t('Type')),
    array('data' => t('# Nodes')),
    );
  $form['header'] = array('#type' => 'value', '#value' => $header);

  $sql = "SELECT type, count(type) AS numnodes
          FROM {node}
          GROUP BY type
          ORDER BY type";
  $result = db_query($sql);

  $clearing = array();
  $rownum = 0;
  while ($row = db_fetch_object($result)) {
    $clearing[$row->type] = '';

    $form['data'][$rownum]['clearing'] = array('#value' => check_plain(0));
    $form['data'][$rownum]['type'] = array('#value' => check_plain($row->type));
    $form['data'][$rownum]['numnodes'] = array('#value' => check_plain($row->numnodes));
    $rownum++;
  }

  $form['clearing'] = array(
    '#type' => 'checkboxes',
    '#options' => $clearing,
    '#default_value' => array(),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Delete nodes"),
  );
  return $form;
}

/**
 * Theme function for tools page.
 */
function theme_migrate_tools($form) {
  $output = drupal_render($form['description']);
  if (isset($form['data']) && is_array($form['data'])) {
    foreach (element_children($form['data']) as $rownum) {
      $row = array();
      foreach (element_children($form['data'][$rownum]) as $colname) {
        if ($colname == 'clearing') {
          $row[] = drupal_render($form[$colname][$form['data'][$rownum]['type']['#value']]);
          // Throw out the column contents
          drupal_render($form['data'][$rownum][$colname]);
        }
        else {
          $row[] = drupal_render($form['data'][$rownum][$colname]);
        }
      }
      $rows[] = $row;
    }
  }

  $header = $form['header']['#value'];
  if (empty($rows)) {
    $rows[] = array(array('data' => t('No data in the table.'), 'colspan' => count($header)));
  }

  $output .= theme('table', $header, $rows);
  $output .= drupal_render($form);

  return $output;
}

/*
 * Implementation of hook_submit().
 */
function _migrate_tools_form_submit($form, &$form_state) {
  $started = time();
  foreach ($form_state['values']['clearing'] as $type => $value) {
    if ($value) {
      drupal_set_message("Deleting all $type nodes");
      $sql = "SELECT nid FROM {node} WHERE type='%s'";
      $result = db_query($sql, $type);
      while ($row = db_fetch_object($result)) {
        node_delete($row->nid);
      }
    }
  }
}

/**
 * Menu callback for settings page.
 */
function migrate_settings() {
  return drupal_get_form('_migrate_settings_form');
}

/**
 * Form definition for settings page.
 */
function _migrate_settings_form($form_state) {
  $form['enable_cron'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable cron processing'),
    '#description' => t('If this is enabled, any operations checked off on the
      dashboard page will be processed when the cron operation runs. Disable this
      if you only perform migration operations interactively or via drush.'),
    '#default_value' => variable_get('migrate_enable_cron', 0),
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
    '#description' => t('With large and/or complex content sets, getting the <strong>Rows
      in Set</strong> value on the Process page can be time-consuming. Enable caching to
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
  $output .= drupal_render($form['enable_cron']);
  $output .= drupal_render($form['display_timers']);
  $output .= drupal_render($form['cache_counts']);
  $output .= drupal_render($form['refresh_counts']);
  $output .= drupal_render($form);

  return $output;
}

/*
 * Implementation of hook_submit().
 */
function _migrate_settings_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  variable_set('migrate_enable_cron', $values['enable_cron']);
  variable_set('migrate_display_timers', $values['display_timers']);
  variable_set('migrate_hide_help_message', $values['hide_help_message']);
  $original_cache = variable_get('migrate_cache_counts', 0);
  variable_set('migrate_cache_counts', $values['cache_counts']);
  if (isset($values['refresh_counts']) || ($values['cache_counts'] && !$original_cache)) {
    $sql = "SELECT mcsid,view_name,view_args FROM {migrate_content_sets}";
    $result = db_query($sql);
    while ($row = db_fetch_object($result)) {
      $rowcount = _migrate_get_view_count($row->view_name, $row->view_args);
      $sql = "UPDATE {migrate_content_sets}
              SET rowcount=%d WHERE mcsid=%d";
      db_query($sql, $rowcount, $row->mcsid);
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
