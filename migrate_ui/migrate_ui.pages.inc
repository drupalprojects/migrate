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
    array('data' => t('Throughput')),
    array('data' => t('Last imported')),
    );

  $migrations = migrate_migrations();

  $rows = array();
  foreach ($migrations as $migration) {
    $row = array();
    $has_counts = TRUE;
    if (method_exists($migration, 'sourceCount')) {
      $total = $migration->sourceCount();
    }
    else {
      $has_counts = FALSE;
      $total = t('N/A');
    }
    if (method_exists($migration, 'importedCount')) {
      $imported = $migration->importedCount();
    }
    else {
      $has_counts = FALSE;
      $imported = t('N/A');
    }
    if ($has_counts) {
      $unimported = $total - $imported;
    }
    else {
      $unimported = t('N/A');
    }
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

    $row['status'] = $status;
    $row['machinename'] =
      l($migration->machineName, 'admin/migrate/' . $migration->machineName);
    $row['importrows'] = $total;
    $row['imported'] = $imported;
    $row['unimported'] = $unimported;

    if (is_subclass_of($migration, 'Migration')) {
      $query = db_select($migration->messageTableName, 'cnt');
      $query->addExpression('COUNT(*)', 'cnt');
      $num_messages = $query->execute()->fetchField();
      $row['messages'] = $num_messages ? l($num_messages, 'admin/migrate/messages/' . $migration->machineName) : 0;
    }
    else {
      $row['messages'] = t('N/A');
    }
    if (method_exists($migration, 'getLastThroughput')) {
      $rate = $migration->getLastThroughput();
      if ($rate == '') {
        $row['lastthroughput'] = t('Unknown');
      }
      elseif ($status == Migration::STATUS_IDLE) {
        $row['lastthroughput'] = t('!rate/min', array('!rate' => $rate));
      }
      else {
        if ($rate > 0) {
          $row['lastthroughput'] = t('!rate/min, !time remaining', array('!rate' => $rate, '!time' => format_interval((60*$unimported) / $rate)));
        }
        else {
          $row['lastthroughput'] = t('!rate/min, unknown time remaining', array('!rate' => $rate));
        }
      }
    }
    else {
      $row['lastthroughput'] = t('N/A');
    }
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
    '#empty' => t('No migrations'),
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

  $has_mappings = isset($migration->fieldMappings);

  $form = array();

  if ($has_mappings) {
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
  }
  else {
    $form['detail'] = array(
      '#type' => 'fieldset',
    );
  }

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

  if ($has_mappings) {
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
  }

  $form['detail']['overview']['description'] = array(
    '#prefix' => '<div>',
    '#markup' => $migration->description,
    '#suffix' => '</div>',
  );

  if ($has_mappings) {
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
  }
  return $form;
}
