<?php
// $Id$

/**
 * @file
 */

/**
 * Menu callback for dashboard page
 */
function migrate_ui_dashboard() {
  $build = array();

  $build['overview'] = array(
    '#prefix' => '<div>',
    '#markup' => migrate_overview(),
    '#suffix' => '</div>',
  );

  $header = array(
    array('data' => t('Status')),
    array('data' => t('Migration')),
    array('data' => t('Total rows')),
    array('data' => t('Imported')),
    array('data' => t('Unimported')),
    array('data' => t('Messages')),
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
      l($migration->machineName, 'admin/migrate/' . $migration->machineName);
    $row['importrows'] = $total;
    $row['imported'] = $imported;
    $row['unimported'] = $unimported;
    $query = db_select($migration->messageTableName, 'cnt');
    $query->addExpression('COUNT(*)', 'cnt');
    $num_messages = $query->execute()->fetchField();
    $row['messages'] = $num_messages ? l($num_messages, 'admin/migrate/messages/' . $migration->machineName) : 0;
    $row['lastimported'] = $migration->getLastImported();
    $rows[] = $row;
  }

  $build['dashboard'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#attached' => array(
      'css' => array(drupal_get_path('module', 'migrate_ui') . '/migrate_ui.css'),
    ),
  );
  return $build;
}

/**
 * Menu callback for messages page
 */
function migrate_ui_messages($migration) {
  $build = $rows = array();

  $header = array(t('Source ID'), t('Level'), t('Message'));

  if (is_string($migration)) {
    $migration = migration_load($migration);
  }

  $messages = db_select($migration->messageTableName, 'msg')
              ->extend('PagerDefault')
              ->limit(500)
              ->fields('msg')
              ->execute();

  foreach ($messages as $message) {
    $classes[] = $message->level <= Migration::MESSAGE_WARNING ? 'migrate-error' : '';
    $rows[] = array(
      array('data' => $message->sourceid1, 'class' => $classes), // TODO: deal with compound keys
      array('data' => $migration->getMessageLevelName($message->level), 'class' => $classes),
      array('data' => $message->message, 'class' => $classes),
    );
    unset($classes);
  }

  $build['messages'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No messages'),
    '#attached' => array(
      'css' => array(drupal_get_path('module', 'migrate_ui') . '/migrate_ui.css'),
    ),
  );
  $build['migrate_ui_pager'] = array('#theme' => 'pager');
  return $build;
}

/**
 * Menu callback function for migration view page.
 */
function migrate_migration_info($form, $form_state, $migration) {
  if (is_string($migration)) {
    $migration = migration_load($migration);
  }

  $form = array();

  // Identify what destination and source fields are mapped
  foreach ($migration->fieldMappings as $mapping) {
    $sourceFields[$mapping->sourceField] = $mapping->sourceField;
    $destinationFields[$mapping->destinationField] = $mapping->destinationField;
  }

  $form['detail'] = array(
    '#type' => 'vertical_tabs',
    '#attached' => array(
      'js' => array(drupal_get_path('module', 'migrate_ui') . '/migrate_ui.js'),
      'css' => array(drupal_get_path('module', 'migrate_ui') . '/migrate_ui.css'),
    ),
  );

  $form['detail']['overview'] = array(
    '#type' => 'fieldset',
    '#title' => t('Overview'),
    '#group' => 'detail',
  );

  $team = array();
  foreach ($migration->team as $member) {
    $team[$member->group][] = $member->name . ' <' . l($member->emailAddress, 'mailto:' . $member->emailAddress) . '>';
  }

  foreach ($team as $group => $list) {
    $form['detail']['overview'][$group] = array(
      '#theme' => 'item_list',
      '#title' => $group,
      '#attributes' => array('id' => 'team'),
      '#items' => $list,
    );
  }

  if (count($migration->dependencies) > 0) {
    $form['detail']['overview']['dependencies'] = array(
      '#prefix' => '<div>',
      '#markup' => '<strong>' . t('Dependencies: ') . '</strong>' .
         implode(', ', $migration->dependencies),
      '#suffix' => '</div>',
    );
  }

  switch ($migration->systemOfRecord) {
    case Migration::SOURCE:
      $system_of_record = t('Source data');
      break;
    case Migration::DESTINATION:
      $system_of_record = t('Destination data');
      break;
    default:
      $system_of_record = t('Unknown');
      break;
  }
  $form['detail']['overview']['system_of_record'] = array(
    '#prefix' => '<div>',
    '#markup' => '<strong>' . t('System of record: ') . '</strong>' . $system_of_record,
    '#suffix' => '</div>',
  );

  $form['detail']['overview']['description'] = array(
    '#prefix' => '<div>',
    '#markup' => $migration->description,
    '#suffix' => '</div>',
  );

  // Destination field information
  $form['detail']['destination'] = array(
    '#type' => 'fieldset',
    '#title' => t('Destination'),
    '#group' => 'detail',
    '#description' =>
      t('<p>These are the fields available in the destination of this
         migration. The machine names listed here are those available to be used
         as the first parameter to $this->addFieldMapping() in your Migration
         class constructor. <span class="error">Unmapped fields are red</span>.</p>'),
  );
  $form['detail']['destination']['type'] = array(
    '#type' => 'item',
    '#title' => t('Type'),
    '#markup' => (string)$migration->destination,
  );
  $destKey = $migration->destination->keySchema;
  $header = array(t('Machine name'), t('Description'));
  $rows = array();
  foreach ($migration->destination->fields() as $machine_name => $description) {
    $classes = array();
    if (isset($destKey[$machine_name])) {
      // Identify primary key
      $machine_name .= t(' (PK)');
    }
    else {
      // Add class for mapped/unmapped. Used in summary.
      $classes[] = !isset($destinationFields[$machine_name]) ? 'migrate-error' : '';
    }
    $rows[] = array(array('data' => $machine_name, 'class' => $classes), array('data' => $description, 'class' => $classes));
  }
  $classes = array();

  $form['detail']['destination']['fields'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No fields'),
  );

  // TODO: Get source_fields from arguments
  $form['detail']['source'] = array(
    '#type' => 'fieldset',
    '#title' => t('Source'),
    '#group' => 'detail',
    '#description' =>
      t('<p>These are the fields available from the source of this
         migration. The machine names listed here are those available to be used
         as the second parameter to $this->addFieldMapping() in your Migration
         class constructor. <span class="error">Unmapped fields are red</span>.</p>'),
  );
  $form['detail']['source']['query'] = array(
    '#type' => 'item',
    '#title' => t('Query'),
    '#markup' => '<pre>' . $migration->source . '</pre>',
  );
  $sourceKey = $migration->source->keySchema;
  $header = array(t('Machine name'), t('Description'));
  $rows = array();
  foreach ($migration->source->fields() as $machine_name => $description) {
    if (isset($sourceKey[$machine_name])) {
      // Identify primary key
      $machine_name .= t(' (PK)');
    }
    else {
      // Add class for mapped/unmapped. Used in summary.
      $classes = !isset($sourceFields[$machine_name]) ? 'migrate-error' : '';
    }
    $rows[] = array(array('data' => $machine_name, 'class' => $classes), array('data' => $description, 'class' => $classes));
  }
  $classes = array();

  $form['detail']['source']['fields'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No fields'),
  );

  $header = array(t('Destination'), t('Source'), t('Default'), t('Description'), t('Priority'));

  // First group the mappings
  $descriptions = array();
  $source_fields = $migration->source->fields();
  $destination_fields = $migration->destination->fields();

  foreach ($migration->fieldMappings as $mapping) {
    // Validate source and destination fields actually exist
    if (!is_null($mapping->sourceField) && !isset($source_fields[$mapping->sourceField])) {
      drupal_set_message(t('!source used as source field in mapping but not in
        list of source fields', array('!source' => $mapping->sourceField)),
      'warning');
    }
    if (!is_null($mapping->destinationField) && !isset($destination_fields[$mapping->destinationField])) {
      drupal_set_message(t('!destination used as destination field in mapping but not in
        list of destination fields', array('!destination' => $mapping->destinationField)),
      'warning');
    }
    $descriptions[$mapping->issueGroup][] = $mapping;
  }

  // Put out each group header
  $rows = array();
  $count = 0;
  foreach ($descriptions as $group => $mappings) {
    $form['detail'][$group] = array(
      '#type' => 'fieldset',
      '#title' => t('Mapping: ') . $group,
      '#group' => 'detail',
      '#attributes' => array('class' => array('migrate-mapping')),
    );
    $rows = array();
    foreach ($mappings as $mapping) {
      if (is_array($mapping->defaultValue)) {
        $default = implode(',', $mapping->defaultValue);
      }
      else {
        $default = $mapping->defaultValue;
      }
      if (isset($mapping->issuePriority)) {
        $classes[] = 'migrate-priority-' . $mapping->issuePriority;
        $priority = MigrateFieldMapping::$priorities[$mapping->issuePriority];
        if (isset($migration->issuePattern) && isset($mapping->issueNumber)) {
          $priority .= ' (' . l('#' . $mapping->issueNumber, str_replace(':id:', $mapping->issueNumber,
            $migration->issuePattern)) . ')';
        }
        if ($mapping->issuePriority != MigrateFieldMapping::ISSUE_PRIORITY_OK) {
          $classes[] = 'migrate-error';
        }
      }
      else {
        $priority = t('OK');
        $classes[] = 'migrate-priority-' . 1;
      }
      $row = array(
        array('data' => $mapping->destinationField, 'class' => $classes),
        array('data' => $mapping->sourceField, 'class' => $classes),
        array('data' => $default, 'class' => $classes),
        array('data' => $mapping->descriptionText,  'class' => $classes),
        array('data' => $priority, 'class' => $classes),
      );
      $rows[] = $row;
      $classes = array();
    }
    $form['detail'][$group]['table'] = array(
      '#theme' => 'table',
      '#header' => $header,
      '#rows' => $rows,
    );
  }
  return $form;
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
