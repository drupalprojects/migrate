<?php

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
      case MigrationBase::STATUS_IDLE:
        $status = t('Idle');
        break;
      case MigrationBase::STATUS_IMPORTING:
        $status = t('Importing');
        break;
      case MigrationBase::STATUS_ROLLING_BACK:
        $status = t('Rolling back');
        break;
      case MigrationBase::STATUS_STOPPING:
        $status = t('Stopping');
        break;
      case MigrationBase::STATUS_DISABLED:
        $status = t('Disabled');
        break;
      default:
        $status = t('Unknown');
        break;
    }

    $row['status'] = $status;
    $machine_name = $migration->getMachineName();
    $row['machinename'] =
      l($machine_name, 'admin/content/migrate/' . $machine_name);
    $row['importrows'] = $total;
    $row['imported'] = $imported;
    $row['unimported'] = $unimported;

    if (is_subclass_of($migration, 'Migration')) {
      $num_messages = $migration->messageCount();
      $row['messages'] = $num_messages ? l($num_messages, 'admin/content/migrate/messages/' . $machine_name) : 0;
    }
    else {
      $row['messages'] = t('N/A');
    }
    if (method_exists($migration, 'getLastThroughput')) {
      $rate = $migration->getLastThroughput();
      if ($rate == '') {
        $row['lastthroughput'] = t('Unknown');
      }
      elseif ($status == MigrationBase::STATUS_IDLE) {
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

  // TODO: need a general MigrateMap API
  $messages = db_select($migration->getMap()->getMessageTable(), 'msg')
              ->extend('PagerDefault')
              ->limit(500)
              ->fields('msg')
              ->execute();

  foreach ($messages as $message) {
    $classes[] = $message->level <= MigrationBase::MESSAGE_WARNING ? 'migrate-error' : '';
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

  $has_mappings = method_exists($migration, 'getFieldMappings');
  $form = array();

  if ($has_mappings) {
    $field_mappings = $migration->getFieldMappings();
    // Identify what destination and source fields are mapped
    foreach ($field_mappings as $mapping) {
      $source_field = $mapping->getSourceField();
      $destination_field = $mapping->getDestinationField();
      $sourceFields[$source_field] = $source_field;
      $destinationFields[$destination_field] = $destination_field;
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

  $form['overview'] = array(
    '#type' => 'fieldset',
    '#title' => t('Overview'),
    '#group' => 'detail',
  );

  $team = array();
  foreach ($migration->getTeam() as $member) {
    $email_address = $member->getEmailAddress();
    $team[$member->getGroup()][] =
      $member->getName() . ' <' . l($email_address, 'mailto:' .$email_address) . '>';
  }

  foreach ($team as $group => $list) {
    $form['overview'][$group] = array(
      '#theme' => 'item_list',
      '#title' => $group,
      '#attributes' => array('id' => 'team'),
      '#items' => $list,
    );
  }

  $dependencies = $migration->getHardDependencies();
  if (count($dependencies) > 0) {
    $form['overview']['dependencies'] = array(
      '#prefix' => '<div>',
      '#markup' => '<strong>' . t('Dependencies: ') . '</strong>' .
         implode(', ', $dependencies),
      '#suffix' => '</div>',
    );
  }
  $soft_dependencies = $migration->getSoftDependencies();
  if (count($soft_dependencies) > 0) {
    $form['overview']['soft_dependencies'] = array(
      '#prefix' => '<div>',
      '#markup' => '<strong>' . t('Soft Dependencies: ') . '</strong>' .
         implode(', ', $soft_dependencies),
      '#suffix' => '</div>',
    );
  }

  if ($has_mappings) {
    switch ($migration->getSystemOfRecord()) {
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
    $form['overview']['system_of_record'] = array(
      '#prefix' => '<div>',
      '#markup' => '<strong>' . t('System of record: ') . '</strong>' . $system_of_record,
      '#suffix' => '</div>',
    );
  }

  $form['overview']['description'] = array(
    '#prefix' => '<div>',
    '#markup' => $migration->getDescription(),
    '#suffix' => '</div>',
  );

  if ($has_mappings) {
    // Destination field information
    $form['destination'] = array(
      '#type' => 'fieldset',
      '#title' => t('Destination'),
      '#group' => 'detail',
      '#description' =>
        t('<p>These are the fields available in the destination of this
           migration. The machine names listed here are those available to be used
           as the first parameter to $this->addFieldMapping() in your Migration
           class constructor. <span class="error">Unmapped fields are red</span>.</p>'),
    );
    $destination = $migration->getDestination();
    $form['destination']['type'] = array(
      '#type' => 'item',
      '#title' => t('Type'),
      '#markup' => (string)$destination,
    );
    $destKey = $destination->getKeySchema();
    $header = array(t('Machine name'), t('Description'));
    $rows = array();
    foreach ($destination->fields() as $machine_name => $description) {
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

    $form['destination']['fields'] = array(
      '#theme' => 'table',
      '#header' => $header,
      '#rows' => $rows,
      '#empty' => t('No fields'),
    );

    // TODO: Get source_fields from arguments
    $form['source'] = array(
      '#type' => 'fieldset',
      '#title' => t('Source'),
      '#group' => 'detail',
      '#description' =>
        t('<p>These are the fields available from the source of this
           migration. The machine names listed here are those available to be used
           as the second parameter to $this->addFieldMapping() in your Migration
           class constructor. <span class="error">Unmapped fields are red</span>.</p>'),
    );
    $source = $migration->getSource();
    $form['source']['query'] = array(
      '#type' => 'item',
      '#title' => t('Query'),
      '#markup' => '<pre>' . $source . '</pre>',
    );
    $sourceKey = $migration->getMap()->getSourceKey();
    $header = array(t('Machine name'), t('Description'));
    $rows = array();
    foreach ($source->fields() as $machine_name => $description) {
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

    $form['source']['fields'] = array(
      '#theme' => 'table',
      '#header' => $header,
      '#rows' => $rows,
      '#empty' => t('No fields'),
    );

    $header = array(t('Destination'), t('Source'), t('Default'), t('Description'), t('Priority'));

    // First group the mappings
    $descriptions = array();
    $source_fields = $source->fields();
    $destination_fields = $destination->fields();

    foreach ($field_mappings as $mapping) {
      // Validate source and destination fields actually exist
      $source_field = $mapping->getSourceField();
      $destination_field = $mapping->getDestinationField();
      if (!is_null($source_field) && !isset($source_fields[$source_field])) {
        drupal_set_message(t('!source used as source field in mapping but not in
          list of source fields', array('!source' => $source_field)),
        'warning');
      }
      if (!is_null($destination_field) && !isset($destination_fields[$destination_field])) {
        drupal_set_message(t('!destination used as destination field in mapping but not in
          list of destination fields', array('!destination' => $destination_field)),
        'warning');
      }
      $descriptions[$mapping->getIssueGroup()][] = $mapping;
    }

    // Put out each group header
    $rows = array();
    $count = 0;
    foreach ($descriptions as $group => $mappings) {
      $form[$group] = array(
        '#type' => 'fieldset',
        '#title' => t('Mapping: ') . $group,
        '#group' => 'detail',
        '#attributes' => array('class' => array('migrate-mapping')),
      );
      $rows = array();
      foreach ($mappings as $mapping) {
        $default = $mapping->getDefaultValue();
        if (is_array($default)) {
          $default = implode(',', $default);
        }
        $issue_priority = $mapping->getIssuePriority();
        if (!is_null($issue_priority)) {
          $classes[] = 'migrate-priority-' . $issue_priority;
          $priority = MigrateFieldMapping::$priorities[$issue_priority];
          $issue_pattern = $migration->getIssuePattern();
          $issue_number = $mapping->getIssueNumber();
          if (!is_null($issue_pattern) && !is_null($issue_number)) {
            $priority .= ' (' . l('#' . $issue_number, str_replace(':id:', $issue_number,
              $issue_pattern)) . ')';
          }
          if ($issue_priority != MigrateFieldMapping::ISSUE_PRIORITY_OK) {
            $classes[] = 'migrate-error';
          }
        }
        else {
          $priority = t('OK');
          $classes[] = 'migrate-priority-' . 1;
        }
        $row = array(
          array('data' => $mapping->getDestinationField(), 'class' => $classes),
          array('data' => $mapping->getSourceField(), 'class' => $classes),
          array('data' => $default, 'class' => $classes),
          array('data' => $mapping->getDescription(),  'class' => $classes),
          array('data' => $priority, 'class' => $classes),
        );
        $rows[] = $row;
        $classes = array();
      }
      $form[$group]['table'] = array(
        '#theme' => 'table',
        '#header' => $header,
        '#rows' => $rows,
      );
    }
  }
  return $form;
}
