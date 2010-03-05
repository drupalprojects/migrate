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
    '#markup' => t('The current status of each migration defined in this system.
      Click on a migration name for details on its configuration.'),
    '#suffix' => '</div>',
  );

  $form['overview'] = array(
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
    $row['lastimported'] = $migration->getLastImported();
    $rows[] = $row;
  }

  $form['dashboard'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
  );
  return $form;
}

/**
 * Menu callback function for migration view page.
 */
function migrate_migration_info($form, $form_state, $migration) {
  if (is_string($migration)) {
    $migration = migration_load($migration);
  }

  $form = array();

  $rows = array();
  foreach ($migration->owners as $member) {
    $row = array(t('Owner:'), $member->name,
      l($member->emailAddress, 'mailto:' . $member->emailAddress));
    $rows[] = $row;
  }
  foreach ($migration->implementors as $member) {
    $row = array(t('Implementor:'), $member->name,
      l($member->emailAddress, 'mailto:' . $member->emailAddress));
    $rows[] = $row;
  }
  $form['members'] = array(
    '#theme' => 'table',
    '#rows' => $rows,
  );

  $form['description'] = array(
    '#prefix' => '<div>',
    '#markup' => $migration->description,
    '#suffix' => '</div>',
  );

  // Identify what destination and source fields are mapped
  foreach ($migration->fieldMappings as $mapping) {
    $sourceFields[$mapping->sourceField] = $mapping->sourceField;
    $destinationFields[$mapping->destinationField] = $mapping->destinationField;
  }

  // Destination and source field information
  $destination_description = '<p>' . (string)$migration->destination . '</p>';
  $form['destination'] = array(
    '#type' => 'fieldset',
    '#title' => t('Destination'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => $destination_description .
      t('<p>These are the fields available in the destination of this
         migration. The machine names listed here are those available to be used
         as the first parameter to $this->addFieldMapping() in your Migration
         class constructor. <strong>Primary key fields</strong> and
         <strong><em>unmapped fields</em></strong> are highlighted.</p>'),
  );
  $destKey = $migration->destination->keySchema;
  $header = array(t('Machine name'), t('Description'));
  $rows = array();
  foreach ($migration->destination->fields() as $machine_name => $description) {
    if (isset($destKey[$machine_name])) {
      $machine_name = "<strong>$machine_name</strong>";
    }
    elseif (!isset($destinationFields[$machine_name])) {
      $machine_name = "<strong><em>$machine_name</em></strong>";
    }
    $rows[] = array($machine_name, $description);
  }

  $form['destination']['fields'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
  );

  // TODO: Get source_fields from arguments
  $source_description = '<div>' . (string)$migration->source . '</div>';
  $form['source'] = array(
    '#type' => 'fieldset',
    '#title' => t('Source'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => $source_description .
      t('<p>These are the fields available from the source of this
         migration. The machine names listed here are those available to be used
         as the second parameter to $this->addFieldMapping() in your Migration
         class constructor. <strong>Primary key fields</strong> and
         <strong><em>unmapped fields</em></strong> are highlighted.</p>'),
  );
  $sourceKey = $migration->source->keySchema;
  $header = array(t('Machine name'), t('Description'));
  $rows = array();
  foreach ($migration->source->fields() as $machine_name => $description) {
    if (isset($sourceKey[$machine_name])) {
      $machine_name = "<strong>$machine_name</strong>";
    }
    elseif (!isset($sourceFields[$machine_name])) {
      $machine_name = "<strong><em>$machine_name</em></strong>";
    }
    $rows[] = array($machine_name, $description);
  }

  $form['source']['fields'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
  );

  // Information on defined mappings
  $form['mappings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Field Mappings'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#description' => t('These are the mappings currently defined for this Migration.
      The <strong>Done</strong> group should indicate those which are fully implemented -
      you can use different group names in field descriptions to classify remainng work.'),
  );
  $header = array(t('Destination'), t('Source'), t('Default'), t('Description'));

  // First group the mappings
  $descriptions = array();
  $source_fields = $migration->source->fields();
  $destination_fields = $migration->destination->fields();

  foreach ($migration->fieldMappings as $mapping) {
    $descriptions[$mapping->descriptionGroup][] = $mapping;
    // While we're running through them, warn about any mappings involving sources
    // or destinations that don't exist (unless marked DNM)
    $lower_group = drupal_strtolower($mapping->descriptionGroup);
    if ($mapping->sourceField && !isset($source_fields[$mapping->sourceField]) &&
        $lower_group != 'dnm' && $lower_group != 'do not migrate') {
      drupal_set_message(t('Field !source_field is mapped, but is not in the source field list.',
        array('!source_field' => $mapping->sourceField)));
    }
    if ($mapping->destinationField && !isset($destination_fields[$mapping->destinationField])) {
      drupal_set_message(t('Field !dest_field is mapped, but is not in the destination field list.',
        array('!dest_field' => $mapping->destinationField)));
    }
  }

  // Put out each group header
  $rows = array();
  $count = 0;
  foreach ($descriptions as $group => $mappings) {
    // Collapse the less interesting cases, and put them last
    $lower_group = drupal_strtolower($group);
    if ($lower_group == 'done' || $lower_group == 'do not migrate' || $lower_group == 'dnm') {
      $collapsed = TRUE;
      if ($lower_group == 'done') {
        $weight = 50;
      }
      else {
        $weight = 51;
      }
    }
    else {
      $collapsed = FALSE;
      $weight = $count;
    }
    $form['mappings'][$group] = array(
      '#type' => 'fieldset',
      '#title' => $group,
      '#collapsible' => TRUE,
      '#collapsed' => $collapsed,
      '#weight' => $weight,
    );
    $rows = array();
    foreach ($mappings as $mapping) {
      if (is_array($mapping->defaultValue)) {
        $default = implode(',', $mapping->defaultValue);
      }
      else {
        $default = $mapping->defaultValue;
      }
      $row = array($mapping->destinationField, $mapping->sourceField,
        $default, $mapping->descriptionText);
      $rows[] = $row;
    }
    $form['mappings'][$group]['grouping'] = array(
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
