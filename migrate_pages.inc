<?php
// $Id$
define('MIGRATE_COLS_ALL', 1);
define('MIGRATE_COLS_EMPTY', 2);
define('MIGRATE_COLS_NONEMPTY', 3);


/**
 * @file
 */

function migrate_front() {
  $content = t('
    <div>Above are tools for managing the migration of external data into Drupal. A typical
         migration process would work as follows:</div>
    <ol><li>Under <strong>Sources</strong>, upload the external data. The raw data is imported
            without filtering into database tables on your server.</li>
        <li>Analyze each imported table. Each column of data in the table is scanned, 
            identifying uniqueness (implying a potential primary key),
            empty columns (automatically hidden by default), possible data types, and data ranges
            and sizes. Each column may be annotated to document its meaning, any special handling, etc.,
            and may be checked Ignore to omit from views and the migration process.</li>
        <li>View the data for each imported table, to aid in the analysis and to mark Excluded
            any rows you don\'t want to import.</li>
        <li>Under <strong>Destinations</strong>, define mappings from the imported data to
            Drupal objects (nodes, users, comments; plus any other types you define through hooks).
            First specify an imported table (with optional SQL selection criteria) and destination
            kind (node, user, comment) and type (story, page, etc.). Then map fields in the imported
            table to fields in the Drupal object.</li>
        <li>Under <strong>Process</strong>, start the actual migration of data into Drupal and
            monitor its progress (if necessary, the processing will continue via cron). Migrated
            data may be cleared and re-imported any number of times as you refine the migration
            process.</li>
    </ol></div>            
    ');
  module_load_include('inc', 'system', 'system.admin');
  return system_admin_menu_block_page().$content;
}

function migrate_dashboard() {
  return drupal_get_form('_migrate_dashboard_form');
}

function _migrate_dashboard_form($form_state) {
  $form['description'] = array(
    '#prefix' => '<div>',
    '#value' => t('View and manage import processes here. Processes that are in progress are checked -
      they can be cancelled by unchecking, or new processes begun by checking, then clicking Submit.
      Any checked process will run in the background (via cron) automatically - you may also run
      them interactively.'),
    '#suffix' => '</div>',
  );

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
          ORDER BY weight, contenttype, view_name";
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
      $maptable = _migrate_map_table_name($row->view_name);
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
        $row->rowcount = _migrate_get_view_count($view);
      }
                                       
      $unimported = $row->rowcount - $imported;

      $msgtablename = _migrate_message_table_name($row->view_name);
      if ($unimported > 0) {
        $messages = '';
        /* Restore when views integration is fully implemented
        $unimported = l($unimported, 'admin/content/migrate/audit/'.$row->view_name.'_unimported');*/
        $numerrors = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                         FROM {" . $msgtablename . "}
                                         WHERE level=%d",
                                        MIGRATE_MESSAGE_ERROR));
        if ($numerrors > 0) {
          $messages = format_plural($numerrors, '1&nbsp;error', '@count&nbsp;errors').'<br />';
          /* Restore when errors are integrated with views
            " (".l($numerrors. ' errors', 'admin/content/migrate/view/'.$row->importtable.'/errors',
            array('html' => TRUE, 'attributes' => array('class' => 'error_link'))).')';*/
        }
        $numwarnings = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                           FROM {" . $msgtablename . "}
                                           WHERE level=%d",
                                          MIGRATE_MESSAGE_WARNING));
        if ($numwarnings > 0) {
          $messages .= format_plural($numwarnings, '1&nbsp;warning', '@count&nbsp;warnings').'<br />';
        }
        $numnotices = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                           FROM {" . $msgtablename . "}
                                           WHERE level=%d",
                                          MIGRATE_MESSAGE_NOTICE));
        if ($numnotices > 0) {
          $messages .= format_plural($numnotices, '1&nbsp;notice', '@count&nbsp;notices').'<br />';
        }
        if ($messages) {
          $unimported = $messages." $unimported&nbsp;total";
        }
      }
      if ($imported > 0) {
        $numinformational = db_result(db_query("SELECT COUNT(DISTINCT sourceid)
                                           FROM {" . $msgtablename . "}
                                           WHERE level=%d",
                                          MIGRATE_MESSAGE_INFORMATIONAL));
        if ($numinformational > 0) {
          $imported .= format_plural($numinformational, 
            '<br />(1 informational message)', 
            '<br />(@count informational messages)').'<br />';
        }
      }
    } else {
      $imported = '';
      $unimported = '';
    }

    $form['data'][$rownum]['clearing'] = array('#value' => check_plain($row->clearing));
    $form['data'][$rownum]['importing'] = array('#value' => check_plain($row->importing));
    $form['data'][$rownum]['scanning'] = array('#value' => check_plain($row->scanning));
    $form['data'][$rownum]['description'] = array('#value' => 
      l($row->description, 'admin/content/migrate/destinations/'.$row->mcsid, 
        array('html' => TRUE)));
    $form['data'][$rownum]['view_name'] = array('#value' => 
      l($row->view_name, 'admin/build/views/edit/'. $row->view_name, 
        array('html' => TRUE)));
    $form['data'][$rownum]['importrows'] = array('#value' => check_plain($row->rowcount));
    $form['data'][$rownum]['imported'] = array('#value' => $imported);
    $form['data'][$rownum]['unimported'] = array('#value' => $unimported);
    $form['data'][$rownum]['lastimported'] = array('#value' => $row->lastimported);
    $form['data'][$rownum]['mcsid'] = array('#value' => check_plain($row->mcsid));
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
      '#description' => t('While large migration tasks are best run in the background (via cron),
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
    $form['clearsemaphore'] = array(
      '#type' => 'checkbox',
      '#title' => t('Unstick migration process'),
      '#description' => t('An error during processing can leave the migration semaphore enabled,
        preventing further processing. When this happens, you will see the message "There is an
        import process already in progress" when you are sure there is no process currently running
        (even in cron). Check this box to clear the semaphore when this happens.'),
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t("Submit"),
    );
  }
  return $form;
}

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
        } else {
          $row[] = drupal_render($form['data'][$rownum][$colname]);
        }
      }
      $rows[] = $row;
    }
  }
  
  $header = $form['header']['#value'];
  if (!$rows) {
    $rows[] = array(array('data' => t('No data in the table.'), 'colspan' => count($header)));
  }
  
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($form);

  return $output;
}

/*
 * Implementation of hook_submit()
 */
function _migrate_dashboard_form_submit($form, &$form_state) {
  $started = time();
  foreach ($form_state['values']['clearing'] as $mcsid => $value) {
    if ($value) {
      $clearing = TRUE;
    } else {
      $clearing = FALSE;
    }
    if ($form_state['values']['importing'][$mcsid]) {
      $importing = TRUE;
    } else {
      $importing = FALSE;
    }
    if ($form_state['values']['scanning'][$mcsid]) {
      $scanning = TRUE;
    } else {
      $scanning = FALSE;
    }
    db_query("UPDATE {migrate_content_sets}
              SET clearing=%d, importing=%d, scanning=%d
              WHERE mcsid=%d",
             $clearing, $importing, $scanning, $mcsid);
  }
  if ($form_state['values']['clearsemaphore']) {
    variable_del('migrate_semaphore');
  }
  if ($form_state['values']['donow']) {
    variable_set('migrate_limit', $form_state['values']['limit']);
    $batch = array(
      'operations' => array(
        array('migrate_content_process_all', array($started, $form_state['values']['limit'],
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
 * @param $results
 * @param $operations
 */
function _migrate_content_process_finish($success, $results, $operations) {
  foreach ($results as $result) {
    drupal_set_message($result);
  }
}

function migrate_destinations() {
  return(drupal_get_form('_migrate_destination_form'));
}

function _migrate_destination_form($form_state) {
  // Fetch information on available destinations
  $desttypes = migrate_invoke_all('destination', 'types');

  $form['description'] = array(
    '#prefix' => '<div>',
    '#value' => t('Define sets of mappings from imported tables to Drupal content.'),
    '#suffix' => '</div>',
  );
  $header = array(
    array('data' => t('Description'), 'field' => 'mcs.description'),
    array('data' => t('Source view'), 'field' => 'mcs.view_name'),
    array('data' => t('Destination')),
//    array('data' => t('End type'), 'field' => 'mcs.desttype'),
    array('data' => t('Weight'), 'field' => 'mcs.weight', 'sort' => 'asc'),
    array('data' => t('')),
  );

  $sql = "SELECT mcs.*
          FROM {migrate_content_sets} mcs
          ORDER BY mcs.weight";
  $result = db_query($sql);
  $used_views = array();
  while ($row = db_fetch_object($result)) {
    if ($row->desttype) {
      $destination = $desttypes[$row->contenttype . '/' . $row->desttype];
    }
    else {
      $destination = $desttypes[$row->contenttype];
    }
    $rows[] = array('data' =>
      array(
        $row->description,
        $row->view_name,
        $destination,
        $row->weight,
        l(t('Edit'), 'admin/content/migrate/destinations/'. $row->mcsid, array('html' => TRUE)),
      ),
      'class' => "migrate-content-sets-tr",
    );
    $used_views[$row->view_name] = $row->view_name;
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
  );
  
  $views = views_get_all_views();
  foreach ($views as $name => $view) {
    if (!isset($used_views[$name])) {
      if ($view->tag) {
        $options[$name] = $view->tag.': '.$name;
      } else {
        $options[$name] = $name;
      }
      if ($view->description) {
        if (strlen($view->description) > 60) {
          $view->description = substr($view->description, 0, 57).'...';
        }
        $options[$name] .= " ($view->description)";
      }
    }
  }
  asort($options);
  $form['addset']['sourceview'] = array(
    '#type' => 'select',
    '#title' => t('Source view from which to import content'),
    '#options' => $options,
  );

  $form['addset']['contenttype'] = array(
    '#type' => 'select',
    '#title' => t('Destination'),
    '#options' => $desttypes,
  );
  
  $form['addset']['weight'] = array(
    '#type' => 'textfield',
    '#title' => t('Weight'),
    '#description' => t('The order in which content sets will be processed and displayed.'),
  );
  $form['addset']['add'] = array(
    '#type' => 'submit',
    '#value' => t('Add'),
  );
  return $form;
}

function _migrate_destination_form_submit($form, &$form_state) {
  $content_set['view_name'] = $form_state['values']['sourceview'];
  $content_set['description'] = $form_state['values']['description'];
  $destination = explode('/', $form_state['values']['contenttype']);
  $content_set['contenttype'] = $destination[0];
  $content_set['desttype'] = $destination[1];
  $content_set['weight'] = $form_state['values']['weight'];

  $mcsid = migrate_save_content_set($content_set);
  // Go straight to the mapping form
  $form_state['redirect'] = "admin/content/migrate/destinations/$mcsid";
}

/**
 * Menu callback function.
 */
function migrate_destination_mappings($form_state, $mcsid) {
  $row = db_fetch_object(db_query("SELECT * FROM {migrate_content_sets}
                                   WHERE mcsid=%d",
                                  $mcsid));
  $desttype = $row->desttype;
  $view_name = $row->view_name;
  $sourcekey = $row->sourcekey;
  $description = $row->description;
  $contenttype = $row->contenttype;
  $desttype = $row->desttype;
  $weight = $row->weight;
  
  // Fetch information on available destinations
  $desttypes = migrate_invoke_all('destination', 'types');
  $destfields = migrate_invoke_all('destination', 'fields', $contenttype, $desttype);
  
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
    '#value' => t('<strong>Source view:</strong> ') . l($view_name, 'admin/build/views/edit/'.$view_name),
    '#suffix' => '</div>',
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
  } else {
    // Need to fill in the query, to find out the aliases that will be returned by the 
    // query
    $view->build();
    $fields = $view->get_items('field');
    $srcoptions = array();
    foreach ($view->query->fields as $fieldalias => $field) {
      $fieldname = $field['field'];
      if (isset($fields[$fieldname]['label']) && $fields[$fieldname]['label']) {
        $srcoptions[$fieldalias] = $fields[$fieldname]['label'];
      } else {
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

function theme_migrate_destination_mappings($form) {
  $output = drupal_render($form['description']);
  $output .= drupal_render($form['show_view_name']);
  $output .= drupal_render($form['show_contenttype']);
  $output .= drupal_render($form['desttype']);
  $output .= drupal_render($form['sourcekey']);
  $output .= drupal_render($form['weight']);

  if (isset($form['destfield']) && is_array($form['destfield'])) {
    foreach(element_children($form['destfield']) as $destfield) {
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
  
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($form['submit']);
  $output .= drupal_render($form);

  return $output;
}

/**
 * Implementation of hook_submit()
 */
function migrate_destination_mappings_submit($form, &$form_state) {
  $mcsid = $form_state['values']['mcsid'];
  if ($form_state['clicked_button']['#parents'][0] == 'delete') {
    migrate_delete_content_set($mcsid);
    drupal_set_message(t('Content set deleted'));
    $form_state['redirect'] = 'admin/content/migrate/destinations';
  } else {
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

function migrate_content_process_all($starttime, $limit = 0, $idlist = '', &$context = NULL) {
  if (variable_get('migrate_semaphore', FALSE)) {
    drupal_set_message('There is an import process already in progress');
    return 0;
  }

  variable_set('migrate_semaphore', TRUE);

  // A zero max_execution_time means no limit - but let's set a reasonable
  // limit anyway
  $maxexectime = ini_get('max_execution_time');
  if (!$maxexectime) {
    $maxexectime = 240;
  }
  
  // Initialize the Batch API context if present
  if ($context) {
    $context['finished'] = 0;
    $starttime = time();
    // The Batch API progress bar will reflect the number of operations being
    // done (clearing/importing/scanning)
    if (!isset($context['sandbox']['numops'])) {
      $numops = 0;
      $sql = "SELECT COUNT(*) FROM {migrate_content_sets} WHERE clearing=1";
      $numops = db_result(db_query($sql));
      $sql = "SELECT COUNT(*) FROM {migrate_content_sets} WHERE importing=1 OR scanning=1";
      $numops += db_result(db_query($sql));
      $context['sandbox']['numops'] = $numops;
      $context['sandbox']['numopsdone'] = 0;
    }
  }

  // Subtract more than enough time to clean up
  $context['sandbox']['timelimit'] = $starttime + $maxexectime - 20;

  // First, perform any clearing actions in reverse order
  $result = db_query("SELECT mcsid
                      FROM {migrate_content_sets}
                      WHERE clearing=1
                      ORDER BY weight DESC");
  $context['sandbox']['timedout'] = FALSE;
  while ($row = db_fetch_object($result)) {
    if ($context['sandbox']['timedout']) {
      break;
    }
    migrate_content_process_clear($row->mcsid, $limit, $idlist, $context);
  }

  // Then, any import actions going forward
  $result = db_query("SELECT mcsid
                      FROM {migrate_content_sets} 
                      WHERE importing=1 OR scanning=1
                      ORDER BY weight");
  while ($row = db_fetch_object($result)) {
    if ($context['sandbox']['timedout']) {
      break;
    }
    migrate_content_process_import($row->mcsid, $limit, $idlist, $context);
  }
  
  // If we did not arrive via a timeout, we must have finished all operations
  if (!$context['sandbox']['timedout']) {
    $context['finished'] = 1;
  }
  else {
    // Not done, report what percentage done we are (in terms of number of operations)
    $context['finished'] = $context['sandbox']['numopsdone']/$context['sandbox']['numops'];
  }

  // When running interactively, if requested save timers for eventual display
  if (variable_get('migrate_display_timers', 0)) {
    global $timers;
    foreach ($timers as $name => $timerec) {
      if (isset($timerec['time'])) {
        $context['sandbox']['times'][$name] += $timerec['time']/1000;
      }
    }
    // When all done, display the timers
    if ($context['finished'] == 1 && isset($context['sandbox']['times'])) {
      global $timers;
      arsort($context['sandbox']['times']);
      foreach ($context['sandbox']['times'] as $name => $total) {
        drupal_set_message("$name: ".round($total,2));
      }
    }
  }
  variable_del('migrate_semaphore');
}

function migrate_tools() {
  return drupal_get_form('_migrate_tools_form');
}

function _migrate_tools_form($form_state) {
  $form['description'] = array(
    '#prefix' => '<div>',
    '#value' => t('Besides content that is migrated into a new site, nodes will be manually
      created during the testing process. Typically you will want to clear these before the
      final migration - if you are <strong>absolutely positive</strong> that all nodes of a
      given type should be deleted, you can do so here.'),
    '#suffix' => '</div>',
  );
  
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
    '#value' => t("Submit"),
  );
  return $form;
}

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
        } else {
          $row[] = drupal_render($form['data'][$rownum][$colname]);
        }
      }
      $rows[] = $row;
    }
  }
  
  $header = $form['header']['#value'];
  if (!$rows) {
    $rows[] = array(array('data' => t('No data in the table.'), 'colspan' => count($header)));
  }
  
  $output .= theme('table', $header, $rows);
  $output .= drupal_render($form);

  return $output;
}

/*
 * Implementation of hook_submit()
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

function migrate_settings() {
  return drupal_get_form('_migrate_settings_form');
}

function _migrate_settings_form($form_state) {
  $form['display_timers'] = array(
    '#type' => 'checkbox',
    '#title' => t('Display timers when processing'),
    '#description' => t('To diagnose performance bottlenecks, turn this toggle
      on - at the completion of a processing round, cumulative times of
      tasks will be displayed.'),
    '#default_value' => variable_get('migrate_display_timers', 0),
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

function theme_migrate_settings($form) {
  $output = drupal_render($form['display_timers']);
  $output .= drupal_render($form['cache_counts']);
  $output .= drupal_render($form['refresh_counts']);
  $output .= drupal_render($form);

  return $output;
}

/*
 * Implementation of hook_submit()
 */
function _migrate_settings_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  variable_set('migrate_display_timers', $values['display_timers']);
  $original_cache = variable_get('migrate_cache_counts', 0);
  variable_set('migrate_cache_counts', $values['cache_counts']);
  if ($values['refresh_counts'] || ($values['cache_counts'] && !$original_cache)) {
    $sql = "SELECT mcsid,view_name FROM {migrate_content_sets}";
    $result = db_query($sql);
    while ($row = db_fetch_object($result)) {
      $rowcount = _migrate_get_view_count($row->view_name);
      $sql = "UPDATE {migrate_content_sets}
              SET rowcount=%d WHERE mcsid=%d";
      db_query($sql, $rowcount, $row->mcsid);
    }
    if ($values['refresh_counts']) {
      drupal_set_message(t('Content set counts refreshed'));
    } else {
      drupal_set_message(t('Content set counts saved'));
    }
  }
  drupal_set_message(t('Settings saved'));
}

function _migrate_get_view_count($view) {
  if (is_string($view)) {
    $view = views_get_view($view);
  }
  // Force execution of count query, with minimal unnecessary results returned
  // @TODO: Find way to execute ONLY count query
  $view->pager['items_per_page'] = 1;
  $view->get_total_rows = TRUE;
  $view->execute(); 
  return $view->total_rows;
}
