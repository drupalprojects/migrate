<?php
/**
 * @file
 * Pages for managing migration processes.
 */
 
/**
 * Form for managing import jobs (migration groups).
 */
function migrate_ui_import_dashboard($form, &$form_state) {
  drupal_set_title(t('Import Jobs'));

  $build = array();

  $build['overview'] = array(
    '#prefix' => '<div>',
    '#markup' => migrate_overview(),
    '#suffix' => '</div>',
  );

  $header = array(
    'machinename' => array('data' => t('Job')),
    'status' => array('data' => t('Status')),
    'source_system' => array('data' => t('Source system')),
  );

  $result = db_select('migrate_group', 'mg')
            ->fields('mg', array('name', 'title', 'arguments'))
            ->execute();
  $rows = array();
  foreach ($result as $group_row) {
    $row = array();
    $migration_result = db_select('migrate_status', 'ms')
                        ->fields('ms', array('machine_name', 'status', 'arguments'))
                        ->condition('group_name', $group_row->name)
                        ->execute();
    if (!$migration_result) {
      continue;
    }
    $status = t('Idle');
    $idle = TRUE;
    $machine_names = array();
    foreach ($migration_result as $migration_row) {
      switch ($migration_row->status) {
        case MigrationBase::STATUS_IMPORTING:
          $status = t('Importing');
          $idle = FALSE;
          break;
        case MigrationBase::STATUS_ROLLING_BACK:
          $status = t('Rolling back');
          $idle = FALSE;
          break;
        case MigrationBase::STATUS_STOPPING:
          $status = t('Stopping');
          $idle = FALSE;
          break;
      }
      $machine_names[] = $migration_row->machine_name;
    }

    // If we're not in the middle of an operaton, what's the status of the import?
    if ($idle) {
      $ready = TRUE;
      $complete = TRUE;
      foreach ($machine_names as $machine_name) {
        $migration = Migration::getInstance($machine_name);
        if (method_exists($migration, 'sourceCount') && method_exists($migration, 'processedCount')) {
          $source_count = $migration->sourceCount();
          $processed_count = $migration->processedCount();
          if ($processed_count > 0) {
            $ready = FALSE;
            if (!$complete) {
              break;
            }
          }
          if ($processed_count != $source_count) {
            $complete = FALSE;
            if (!$ready) {
              break;
            }
          }
        }
      }
      if ($ready) {
        $status = t('Ready to import');
      }
      elseif ($complete) {
        $status = t('Import complete');
      }
      else {
        $status = t('Import incomplete, not currently running');
      }
    }

    $row['status'] = $status;
    $row['machinename'] =
      l($group_row->title, 'admin/content/import/jobs/' . $group_row->name);
    $arguments = unserialize($group_row->arguments);
    if (!empty($arguments['source_system'])) {
      $row['source_system'] = $arguments['source_system'];
    }
    else {
      $row['source_system'] = '';
    }
    $rows[$group_row->name] = $row;
  }

  $build['dashboard']['tasks'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $rows,
    '#tree' => TRUE,
    '#empty' => t('No import jobs defined.'),
  );
  $build['operations'] = _migrate_ui_import_operations();

  return $build;
}

/**
 * Menu callback
 */
function migrate_ui_import_job($form, &$form_state, $group_name) {
  $build = array();

  $header = array(
    'machinename' => array('data' => t('Task')),
    'status' => array('data' => t('Status')),
    'importrows' => array('data' => t('Items')),
    'imported' => array('data' => t('Imported')),
  );
  if (user_access(MIGRATE_ACCESS_ADVANCED)) {
    $header += array(
      'unimported' => array('data' => t('Unimported')),
      'messages' => array('data' => t('Messages')),
      'lastthroughput' => array('data' => t('Throughput')),
      'lastimported' => array('data' => t('Last imported')),
    );
  }

  $migrations = migrate_migrations();

  $rows = array();
  foreach ($migrations as $migration) {
    $current_group = $migration->getGroup()->getName();
    if ($current_group != $group_name) {
      continue;
    }
    $row = array();
    $has_counts = TRUE;
    if (method_exists($migration, 'sourceCount')) {
      $total = $migration->sourceCount();
      if ($total < 0) {
        $has_counts = FALSE;
        $total = t('N/A');
      }
    }
    else {
      $has_counts = FALSE;
      $total = t('N/A');
    }
    if (method_exists($migration, 'importedCount')) {
      $imported = $migration->importedCount();
      $processed = $migration->processedCount();
    }
    else {
      $has_counts = FALSE;
      $imported = t('N/A');
    }
    if ($has_counts) {
      $unimported = $total - $processed;
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
    $group_length = strlen($group_name);
    if (!strncasecmp($group_name, $machine_name, strlen($group_length))) {
      $display_name = substr($machine_name, $group_length);
    }
    else {
      $display_name = $machine_name;
    }
    $row['machinename'] =
      l($display_name, "admin/content/import/jobs/$group_name/$machine_name");
    $row['importrows'] = $total;
    $row['imported'] = $imported;
    if (user_access(MIGRATE_ACCESS_ADVANCED)) {
      $row['unimported'] = $unimported;

      if (is_subclass_of($migration, 'Migration')) {
        $num_messages = $migration->messageCount();
        $row['messages'] = $num_messages ?
          l($num_messages, "admin/content/import/jobs/$group_name/$machine_name/messages")
          : 0;
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
    }
    $rows[$machine_name] = $row;
  }

  $build['dashboard']['migrations'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $rows,
    '#tree' => TRUE,
    '#empty' => t('No tasks are defined for this import job.'),
  );

  $build['operations'] = _migrate_ui_import_operations();

  return $build;
}

/**
 * @return array
 */
function _migrate_ui_import_operations() {
  // Build the 'Update options' form.
  $operations = array(
    '#type' => 'fieldset',
    '#title' => t('Operations'),
  );

  $options = array(
    'import' => t('Import'),
    'rollback' => t('Rollback'),
    'stop' => t('Stop'),
    'reset' => t('Reset'),
    'deregister' => t('Remove import settings'),
  );
  $operations['operation'] = array(
    '#type' => 'select',
    '#title' => t('Operation'),
    '#title_display' => 'invisible',
    '#options' => $options,
  );
  $operations['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Execute'),
    '#validate' => array('migrate_ui_import_validate'),
    '#submit' => array('migrate_ui_import_submit'),
  );
  $operations['description'] = array(
    '#prefix' => '<p>',
    '#markup' => t(
      'Choose an operation to run on all selections above:
       <ul>
         <li>Import<br /> Imports all previously unimported records from the source, plus
             any records marked for update, into destination Drupal objects.</li>
         <li>Rollback<br /> Deletes all Drupal objects created by the import.</li>
         <li>Stop<br /> Cleanly interrupts any import or rollback processes that may
             currently be running.</li>
         <li>Reset<br /> Sometimes a process may fail to stop cleanly, and be
             left stuck in an Importing or Rolling Back status. Choose Reset to clear
             the status and permit other operations to proceed.</li>
         <li>Remove import settings<br /> Removes all information about an import job
             or task, while preserving any content that has already been imported.</li>
       </ul>'
    ),
    '#postfix' => '</p>',
  );

  if (user_access(MIGRATE_ACCESS_ADVANCED)) {
    $operations['options'] = array(
      '#type' => 'fieldset',
      '#title' => t('Options'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    $operations['options']['update'] = array(
      '#type' => 'checkbox',
      '#title' => t('Update'),
      '#description' => t('Check this box to update all previously-imported content
        in addition to importing new content. Leave unchecked to only import
        new content'),
    );
    $operations['options']['force'] = array(
      '#type' => 'checkbox',
      '#title' => t('Ignore dependencies'),
      '#description' => t('Check this box to ignore dependencies when running imports
        - all tasks will run whether or not their dependent tasks have
        completed.'),
    );
    $operations['options']['limit'] = array(
      '#tree' => TRUE,
      '#type' => 'fieldset',
      '#attributes' => array('class' => array('container-inline')),
      'value' => array(
        '#type' => 'textfield',
        '#title' => t('Limit to:'),
        '#size' => 10,
      ),
      'unit' => array(
        '#type' => 'select',
        '#options' => array(
          'items' => t('items'),
          'seconds' => t('seconds'),
        ),
        '#description' => t('Set a limit of how many items to process for
          each import task, or how long each should run.'),
      ),
    );
  }

  return $operations;
}

/**
 * Validate callback for the dashboard form.
 */
function migrate_ui_import_validate($form, &$form_state) {
  $migrations = _migrate_ui_selected_migrations($form_state['values']);
  if (empty($migrations)) {
    if (isset($form_state['values']['migrations'])) {
      form_set_error('', t('No items selected.'));
    }
  }
}

/**
 * Determine what migrations are selected (whether directly on a job page, or
 * via group selections on the dashboard).
 *
 * @param $values
 *
 * @return array
 */
function _migrate_ui_selected_migrations($values) {
  if (isset($values['migrations'])) {
    // From the specific job page, just take them in order (they were already
    // sorted by dependencies).
    $machine_names = array_filter($values['migrations']);
  }
  else {
    // From the jobs page, we need to use migrate_migrations to be sure we've
    // got the tasks for each job in dependency order.
    $tasks = array_filter($values['tasks']);
    $migrations = migrate_migrations();
    $machine_names = array();
    foreach ($migrations as $migration) {
      $group_name = $migration->getGroup()->getName();
      if (in_array($group_name, $tasks)) {
        $machine_names[] = $migration->getMachineName();
      }
    }
  }
  return $machine_names;
}

/**
 * Submit callback for the dashboard form.
 */
function migrate_ui_import_submit($form, &$form_state) {
  $values = $form_state['values'];
  $operation = $values['operation'];
  if (isset($values['limit'])) {
    $limit = $values['limit'];
  }
  else {
    $limit = '';
  }
  if (isset($values['update'])) {
    $update = $values['update'];
  }
  else {
    $update = 0;
  }
  if (isset($values['force'])) {
    $force = $values['force'];
  }
  else {
    $force = 0;
  }
  $machine_names = _migrate_ui_selected_migrations($values);
  $operations = array();

  // Rollback in reverse order.
  if ($operation == 'rollback') {
    $machine_names = array_reverse($machine_names);
  }

  // Special case: when deregistering a group, go through the group API
  if ($operation == 'deregister' && isset($values['tasks'])) {
    foreach ($values['tasks'] as $task) {
      MigrateGroup::deregister($task);
    }
    return;
  }

  foreach ($machine_names as $machine_name) {
    $migration = Migration::getInstance($machine_name);
    switch ($operation) {
      case 'import':
        // Update (if necessary) once, before starting
        if ($update && method_exists($migration, 'prepareUpdate')) {
          $migration->prepareUpdate();
        }
        $operations[] = array('migrate_ui_batch', array('import', $machine_name, $limit, $force));
        break;
      case 'rollback':
        $operations[] = array('migrate_ui_batch', array('rollback', $machine_name, $limit, $force));
        break;
      case 'stop':
        $migration->stopProcess();
        break;
      case 'reset':
        $migration->resetStatus();
        break;
      case 'deregister':
        migrate_ui_deregister_migration($machine_name);
        break;
    }
  }

  // Only rollback and import operations will need to go through Batch API.
  if (count($operations) > 0) {
    $batch = array(
      'operations' => $operations,
      'title' => t('Import processing'),
      'file' => drupal_get_path('module', 'migrate_ui') . '/migrate_ui.pages.inc',
      'init_message' => t('Starting import process'),
      'progress_message' => t(''),
      'error_message' => t('An error occurred. Some or all of the import processing has failed.'),
      'finished' => 'migrate_ui_batch_finish',
    );
    batch_set($batch);
  }
}

/**
 * Process all enabled migration processes in a browser, using the Batch API
 * to break it into manageable chunks.
 *
 * @param $operation
 *  Operation to perform - 'import', 'rollback', 'stop', or 'reset'.
 * @param $machine_name
 *  Machine name of migration to process.
 * @param $limit
 *  An array indicating the number of items to import or rollback, or the
 *  number of seconds to process. Should include 'unit' (either 'items' or
 *  'seconds') and 'value'.
 * @param $context
 *  Batch API context structure
 */
function migrate_ui_batch($operation, $machine_name, $limit, $force = FALSE, &$context) {
  // If we got a stop message, skip everything else
  if (isset($context['results']['stopped'])) {
    $context['finished'] = 1;
    return;
  }

  $migration = Migration::getInstance($machine_name);

  // Messages generated by migration processes will be captured in this global
  global $_migrate_messages;
  $_migrate_messages = array();
  Migration::setDisplayFunction('migrate_ui_capture_message');

  // Perform the requested operation
  switch ($operation) {
    case 'import':
      $result = $migration->processImport(array('limit' => $limit, 'force' => $force));
      break;
    case 'rollback':
      $result = $migration->processRollback(array('limit' => $limit, 'force' => $force));
      break;
  }

  switch ($result) {
    case Migration::RESULT_INCOMPLETE:
      // Default to half-done, in case we can't get a more precise fix
      $context['finished'] = .5;
      if (method_exists($migration, 'sourceCount')) {
        $total = $migration->sourceCount();
        if ($total > 0 && method_exists($migration, 'importedCount')) {
          $processed = $migration->processedCount();
          switch ($operation) {
            case 'import':
              $to_update = $migration->updateCount();
              $context['finished'] = ($processed-$to_update)/$total;
              break;
            case 'rollback':
              $context['finished'] = ($total - $migration->importedCount())/$total;
              break;
          }
        }
      }
      break;
    case MigrationBase::RESULT_SKIPPED:
      $_migrate_messages[] = t("Skipped !name due to unfulfilled dependencies: !depends",
        array(
          '!name' => $machine_name,
          '!depends' => implode(", ", $migration->incompleteDependencies()),
        ));
      $context['finished'] = 1;
      break;
    case MigrationBase::RESULT_STOPPED:
      $context['finished'] = 1;
      // Skip any further operations
      $context['results']['stopped'] = TRUE;
      break;
    default:
      $context['finished'] = 1;
      break;
  }

  // Add any messages generated in this batch to the cumulative list
  foreach ($_migrate_messages as $message) {
    $context['results'][] = $message;
  }

  // While in progress, show the cumulative list of messages
  $full_message = '';
  foreach ($context['results'] as $message) {
    $full_message .= $message . '<br />';
  }
  $context['message'] = $full_message;
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
function migrate_ui_batch_finish($success, $results, $operations) {
  unset($results['stopped']);
  if (user_access(MIGRATE_ACCESS_ADVANCED)) {
    foreach ($results as $result) {
      drupal_set_message($result);
    }
  }
}

function migrate_ui_capture_message($message, $level) {
  if ($level != 'debug') {
    global $_migrate_messages;
    $_migrate_messages[] = $message;
  }
}

/**
 * Menu callback function for migration view page.
 */
function migrate_migration_info($form, $form_state, $group_name, $migration_name) {
  drupal_set_title($migration_name);
  $migration = Migration::getInstance($migration_name);

  $has_mappings = method_exists($migration, 'getFieldMappings');
  $form = array();

  if ($has_mappings) {
    $field_mappings = $migration->getFieldMappings();
    // Identify what destination and source fields are mapped
    foreach ($field_mappings as $mapping) {
      $source_field = $mapping->getSourceField();
      $destination_field = $mapping->getDestinationField();
      $source_fields[$source_field] = $source_field;
      $destination_fields[$destination_field] = $destination_field;
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
      $member->getName() . ' <' . l($email_address, 'mailto:' . $email_address) . '>';
  }

  foreach ($team as $group => $list) {
    $form['overview'][$group] = array(
      '#type' => 'item',
      '#title' => $group,
      '#markup' => implode(', ', $list),
    );
  }

  $dependencies = $migration->getHardDependencies();
  if (count($dependencies) > 0) {
    $form['overview']['dependencies'] = array(
      '#title' => t('Dependencies') ,
      '#markup' => implode(', ', $dependencies),
      '#type' => 'item',
    );
  }
  $soft_dependencies = $migration->getSoftDependencies();
  if (count($soft_dependencies) > 0) {
    $form['overview']['soft_dependencies'] = array(
      '#title' => t('Soft Dependencies'),
      '#markup' => implode(', ', $soft_dependencies),
      '#type' => 'item',
    );
  }

  $form['overview']['group'] = array(
    '#title' => t('Group:'),
    '#markup' => $migration->getGroup()->getTitle(),
    '#type' => 'item',
  );

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
      '#type' => 'item',
      '#title' => t('System of record:'),
      '#markup' => $system_of_record,
    );
  }

  $form['overview']['description'] = array(
    '#title' => t('Description:'),
    '#markup' => $migration->getDescription(),
    '#type' => 'item',
  );

  if ($has_mappings) {
    // Destination field information
    $form['destination'] = array(
      '#type' => 'fieldset',
      '#title' => t('Destination'),
      '#group' => 'detail',
      '#description' =>
        t('<p>These are the fields available in the destination of this import
           task. The machine names listed here are those available to be used
           as the first parameter to $this->addFieldMapping() in your Migration
           class constructor. <span class="error">Unmapped fields are red</span>.</p>'),
    );
    $destination = $migration->getDestination();
    $form['destination']['type'] = array(
      '#type' => 'item',
      '#title' => t('Type'),
      '#markup' => (string)$destination,
    );
    $dest_key = $destination->getKeySchema();
    $header = array(t('Machine name'), t('Description'));
    $rows = array();
    foreach ($destination->fields($migration) as $machine_name => $description) {
      $classes = array();
      if (isset($dest_key[$machine_name])) {
        // Identify primary key
        $machine_name .= ' ' . t('(PK)');
      }
      else {
        // Add class for mapped/unmapped. Used in summary.
        $classes[] = !isset($destination_fields[$machine_name]) ? 'migrate-error' : '';
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
        t('<p>These are the fields available from the source of this import
           task. The machine names listed here are those available to be used
           as the second parameter to $this->addFieldMapping() in your Migration
           class constructor. <span class="error">Unmapped fields are red</span>.</p>'),
    );
    $source = $migration->getSource();
    $form['source']['query'] = array(
      '#type' => 'item',
      '#title' => t('Query'),
      '#markup' => '<pre>' . $source . '</pre>',
    );
    $source_key = $migration->getMap()->getSourceKey();
    $header = array(t('Machine name'), t('Description'));
    $rows = array();
    foreach ($source->fields() as $machine_name => $description) {
      if (isset($source_key[$machine_name])) {
        // Identify primary key
        $machine_name .= ' ' . t('(PK)');
      }
      else {
        // Add class for mapped/unmapped. Used in summary.
        $classes = !isset($source_fields[$machine_name]) ? 'migrate-error' : '';
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
    $destination_fields = $destination->fields($migration);

    foreach ($field_mappings as $mapping) {
      // Validate source and destination fields actually exist
      $source_field = $mapping->getSourceField();
      $destination_field = $mapping->getDestinationField();
      if (!is_null($source_field) && !isset($source_fields[$source_field])) {
        drupal_set_message(t('"!source" was used as source field in the
          "!destination" mapping but is not in list of source fields', array(
            '!source' => $source_field,
            '!destination' => $destination_field
          )),
        'warning');
      }
      if (!is_null($destination_field) && !isset($destination_fields[$destination_field])) {
        drupal_set_message(t('"!destination" was used as destination field in
          "!source" mapping but is not in list of destination fields', array(
            '!source' => $source_field,
            '!destination' => $destination_field)),
        'warning');
      }
      $descriptions[$mapping->getIssueGroup()][] = $mapping;
    }

    // Put out each group header
    foreach ($descriptions as $group => $mappings) {
      $form[$group] = array(
        '#type' => 'fieldset',
        '#title' => t('Mapping: !group', array('!group' => $group)),
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
            $priority .= ' (' . l(t('#') . $issue_number, str_replace(':id:', $issue_number,
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
        $destination_field = $mapping->getDestinationField();
        $source_field = $mapping->getSourceField();
        // Highlight any mappings overridden in the database.
        if ($mapping->getMappingSource() == MigrateFieldMapping::MAPPING_SOURCE_DB) {
          $destination_field = "<em>$destination_field</em>";
          $source_field = "<em>$source_field</em>";
        }
        $row = array(
          array('data' => $destination_field, 'class' => $classes),
          array('data' => $source_field, 'class' => $classes),
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

function migrate_ui_edit_mappings($form, $form_state, $group_name, $migration_name) {
  drupal_set_title(t('Edit field mappings for !migration',
                   array('!migration' => $migration_name)));
  $form = migrate_ui_mapping_edit_form($migration_name);
  $form['machine_name'] = array(
    '#type' => 'value',
    '#value' => $migration_name,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Apply changes'),
  );
  return $form;
}

/**
 * Build a subform for editing the field mappings for the named migration.
 *
 * @param $migration_name
 *
 * @return array
 */
function migrate_ui_mapping_edit_form($migration_name) {
  $migration = Migration::getInstance($migration_name);
  $source_fields = $migration->getSource()->fields();
  $dest_fields = $migration->getDestination()->fields();
  $dest_key = $migration->getDestination()->getKeySchema();
  $field_mappings = $migration->getFieldMappings();

  $form = array();
  $form['#tree'] = TRUE;

  $base_options = array(
    '-1' => t('--Do not import--'),
    // @todo: Not yet supported
//      '0' => t('--Create field--'),
  );
  $field_options = array();
  foreach ($source_fields as $name => $description) {
    if (is_array($description)) {
      $description = reset($description);
    }
    $description = strip_tags($description);
    // Limit the description length
    if (strlen($description) > 50) {
      $description = substr($description, 0, 50) . '...';
    }
    if (user_access(MIGRATE_ACCESS_ADVANCED)) {
      $label = '!description [!source_field]';
    }
    else {
      $label = '!description';
    }
    $field_options[$name] = t($label,
      array('!source_field' => $name, '!description' => $description));
  }

  foreach ($dest_fields as $name => $description) {
    // Don't map the destination key
    if (isset($dest_key[$name])) {
      continue;
    }
    // For now, hide these fields
    $skip_fields = array(
      // node fields
      'comment',
      'daycount',
      'is_new',
      'language',
      'log',
      'revision',
      'revision_uid',
      'timestamp',
      'tnid',
      'totalcount',
      'translate',
      // user fields
      'data',
    );
    if (in_array($name, $skip_fields)) {
      continue;
    }
    // And these subfields
    $skip_suffixes = array(
      // Text fields
      'format',
      'language',
      // Term reference fields
      'source_type',
      'create_term',
      'ignore_case',
      // File fields
      'destination_dir',
      'destination_file',
      'file_replace',
      'preserve_files',
      'source_dir',
      'file_class',
    );
    $parts = explode(':', $name);
    if (isset($parts[1]) && in_array($parts[1], $skip_suffixes)) {
      continue;
    }
    if (is_array($description)) {
      $description = reset($description);
    }

    $options = $base_options + $field_options;
    $default_value = NULL;
    if (isset($field_mappings[$name])) {
      $mapping = $field_mappings[$name];
      if ($mapping->getSourceField()) {
        $default_value = $mapping->getSourceField();
      }
    }
    else {
      $mapping = NULL;
    }

    // If we have a match on field name, default the mapping to that match
    // and remove the option to create a new field of that name.
    if (is_null($default_value)) {
      if (isset($field_options[$name])) {
        $default_value = $name;
        unset($options['0']);
      }
      else {
        $default_value = '-1';
      }
    }

    if (user_access(MIGRATE_ACCESS_ADVANCED)) {
      $label = '!description [!field_name]';
    }
    else {
      $label = '!description';
    }

    $form['field_mappings'][$name]['mapping'] = array(
      '#type' => 'select',
      '#title' => t($label,
                    array('!description' => $description, '!field_name' => $name)),
      '#options' => $options,
      '#default_value' => $default_value,
    );

    if ($mapping) {
      $field_default = $mapping->getDefaultValue();
    }
    else {
      $field_default = '';
    }
    $form['field_mappings'][$name]['default_value'] = array(
      '#type' => 'textfield',
      '#default_value' => $field_default,
      '#size' => 20,
    );
  }

  // Set our theme function for this form
  $form['field_mappings']['#theme'][] = 'migrate_ui_field_mapping_form';

  return $form;
}

function migrate_ui_edit_mappings_submit(&$form, &$form_state) {
  $machine_name = $form_state['values']['machine_name'];
  $row = db_select('migrate_status', 'ms')
               ->fields('ms', array('arguments', 'class_name', 'group_name'))
               ->condition('machine_name', $machine_name)
               ->execute()
               ->fetchObject();
  $class_name = $row->class_name;
  $group_name = $row->group_name;
  $arguments = unserialize($row->arguments);
  $arguments['field_mappings'] = array();
  $field_mappings = array();
  $default_values = array();

  $migration = Migration::getInstance($machine_name);
  $existing_mappings = $migration->getFieldMappings();

  foreach ($form_state['values']['field_mappings'] as $destination_field => $info) {
    // If this mapping matches a coded mapping but not a stored mapping, remove it entirely (don't
    // store it in the database) so the coded mapping is not overwritten.
    if (isset($coded_mappings[$destination_field])) {
      if ($info['mapping'] == $coded_mappings[$destination_field]->getSourceField() &&
          $info['default_value'] == $coded_mappings[$destination_field]->getDefaultValue) {
        continue;
      }
    }
    $field_mappings[$destination_field] = $info['mapping'];
    $default_values[$destination_field] = $info['default_value'];
  }
  foreach ($field_mappings as $destination_field => $source_field) {
    if ($source_field == -1) {
      $source_field = NULL;
    }

    // Until we can provide editing of all the options that go along with
    // field mappings, we want to avoid overwriting pre-existing mappings and
    // losing important bits like sourceMigration.
    $mapping = NULL;
    if (isset($existing_mappings[$destination_field])) {
      $old_mapping = $existing_mappings[$destination_field];
      if ($source_field == $old_mapping->getSourceField() &&
          $default_values[$destination_field] == $old_mapping->getDefaultValue) {
        // First, if this mapping matches a previously-stored mapping, we want to
        // preserve it as it was originally stored.
        if ($old_mapping->getMappingSource() == MigrateFieldMapping::MAPPING_SOURCE_DB) {
          $mapping = $old_mapping;
        }
        // If it matches a coded mapping, then we don't want to save it at all.
        else {
          continue;
        }
      }
    }
    // We're not skipping this mapping, or preserving an old one, so create the
    // new mapping.
    if (!$mapping) {
      $mapping = new MigrateFieldMapping($destination_field, $source_field);
      $mapping->defaultValue($default_values[$destination_field]);
    }
    // Special handling for term references
    $destination_title = $form['field_mappings'][$destination_field]['mapping']['#title'];
    if (strstr($destination_title, '(taxonomy_term_reference)')) {
      // Assume the source field is going to reference a term migration
      if (!empty($source_field)) {
        // Find the destination vocabulary
        $field_info = field_info_field($destination_field);
        $destination_vocab = $field_info['settings']['allowed_values'][0]['vocabulary'];
        $term_migrations = array();
        // Look for term migrations
        foreach (migrate_migrations() as $term_machine_name => $term_migration) {
          $destination = $term_migration->getDestination();
          if (is_a($destination, 'MigrateDestinationTerm') &&
              ($destination->getBundle() == $destination_vocab) &&
              ($term_migration->getGroup()->getName() == $group_name)) {
            $term_migrations[] = $term_machine_name;
          }
        }
        if (!empty($term_migrations)) {
          $mapping->sourceMigration($term_migrations);
          $arguments['field_mappings'][] = $mapping;
          $type_mapping = new MigrateFieldMapping("$destination_field:source_type", NULL);
          $type_mapping->defaultValue('tid');
          $arguments['field_mappings'][] = $type_mapping;
        }
      }
      elseif (!is_null($default_values[$destination_field])) {
        $arguments['field_mappings'][] = $mapping;
        // Always allow creation of terms
        $mapping = new MigrateFieldMapping("$destination_field:create_term", NULL);
        $mapping->defaultValue(1);
        $arguments['field_mappings'][] = $mapping;
      }
    }
    // Special handling for files
    elseif (strpos($destination_title, '(image)') ||
            strpos($destination_title, '(file)') ||
            strpos($destination_title, '(media)')) {
      // Assume we're migrating from fids, unless it's a media video field, in
      // which case use the Youtube class.
      $file_class = 'MigrateFileFid';
      if (module_exists('media')) {
        $destination = $migration->getDestination();
        if (is_a($destination, 'MigrateDestinationEntity')) {
          $field_instance = field_info_instance($destination->getEntityType(),
            $destination_field, $destination->getBundle());
          if ($field_instance['widget']['settings']['allowed_types']['video'] == 'video') {
            $file_class = 'MigrateExtrasFileYoutube';
          }
        }
      }
      if ($file_class == 'MigrateFileFid') {
        $mapping->sourceMigration($group_name . 'File');
      }
      $arguments['field_mappings'][] = $mapping;
      $mapping = new MigrateFieldMapping("$destination_field:file_class", NULL);
      $mapping->defaultValue($file_class);
      $arguments['field_mappings'][] = $mapping;
    }
    else {
      $arguments['field_mappings'][] = $mapping;
    }
  }

  Migration::registerMigration($class_name, $machine_name, $arguments, $group_name);

  drupal_set_message(t('Field mapping changes applied.'));
  $form_state['redirect'] = "admin/content/import/jobs/$group_name/$machine_name";
}

/**
 * Theme function to layout field mappings in a table.
 *
 * @param array $variables
 *
 * @return string
 *  Rendered markup.
 */
function theme_migrate_ui_field_mapping_form($variables) {
  $output = '';
  $form = $variables['field_mappings'];
  $elements = element_children($form);
  if (!empty($elements)) {
    $header = array(t('Destination field'), t('Source field'), t('Default value'));
    $rows = array();
    foreach ($elements as $mapping_key) {
      $row = array();
      $title = $form[$mapping_key]['mapping']['#title'];
      unset($form[$mapping_key]['mapping']['#title']);
      $row[] = $title;
      $row[] = drupal_render($form[$mapping_key]['mapping']);
      $row[] = drupal_render($form[$mapping_key]['default_value']);
      $rows[] = $row;
    }
    $output .= theme('table', array('header' => $header, 'rows' => $rows));
  }
  $output .= drupal_render_children($form);
  return $output;
}

/**
 * Menu callback for messages page
 */
function migrate_ui_messages($group_name, $migration_name) {
  drupal_set_title(t('Import messages for !migration',
                   array('!migration' => $migration_name)));

  $build = $rows = array();

  $header = array(
    array('data' => t('Source ID'), 'field' => 'sourceid1', 'sort' => 'asc'),
    array('data' => t('Level'), 'field' => 'level'),
    array('data' => t('Message'), 'field' => 'message'),
  );

  $migration = Migration::getInstance($migration_name);

  // TODO: need a general MigrateMap API
  $messages = $migration->getMap()->getConnection()
              ->select($migration->getMap()->getMessageTable(), 'msg')
              ->extend('PagerDefault')
              ->extend('TableSort')
              ->orderByHeader($header)
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
 * Menu callback
 */
function migrate_ui_registration() {
  drupal_set_title(t('Class registration'));
  return drupal_get_form('migrate_ui_registration_form');
}

/**
 * Form for reviewing migrations.
 */
function migrate_ui_registration_form($form, &$form_state) {
  $build = array();

  $description = t('To register (or reregister with updated arguments) any
    migrations defined in hook_migrate_api(), click the <strong>Register</strong>
    button below.');

  $build['registration'] = array(
    '#type' => 'fieldset',
    '#title' => t('Registration'),
    '#description' => $description,
  );

  $build['registration']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Register statically-defined classes'),
    '#submit' => array('migrate_ui_configure_register_submit'),
  );

  $migrations = array();
  $result = db_select('migrate_status', 'ms')
                ->fields('ms', array('class_name', 'machine_name'))
                ->execute();
  $migration_list = '';
  foreach ($result as $row) {
    if (!class_exists($row->class_name)) {
      $migrations[] = $row->machine_name;
      $migration_list .= '<li>' . t('!migration (class !class)',
        array('!migration' => $row->machine_name, '!class' => $row->class_name)) . "</li>\n";
    }
  }

  if (!empty($migrations)) {
    $description = t('No class currently exists for the following migrations: <ul>!list</ul>',
      array('!list' => $migration_list));
    $build['deregistration'] = array(
      '#type' => 'fieldset',
      '#title' => t('Orphaned import tasks'),
      '#description' => $description,
    );
    $build['deregistration']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Deregister orphans'),
      '#submit' => array('migrate_ui_configure_deregister_submit'),
    );
  }

  return $build;
}

/**
 * Submit callback for the configuration form registration fieldset.
 */
function migrate_ui_configure_register_submit($form, &$form_state) {
  migrate_static_registration();
  drupal_set_message(t('All statically defined migrations have been (re)registered.'));
  $form_state['redirect'] = 'admin/content/import';
}

/**
 * Submit callback for the configuration form deregistration fieldset.
 */
function migrate_ui_configure_deregister_submit($form, &$form_state) {
  $result = db_select('migrate_status', 'ms')
                ->fields('ms', array('class_name', 'machine_name'))
                ->execute();
  foreach ($result as $row) {
    if (!class_exists($row->class_name)) {
      migrate_ui_deregister_migration($row->machine_name);
    }
  }
  $form_state['redirect'] = 'admin/content/import';
}

/**
 * Given a migration machine name, remove its tracking from the database.
 *
 * @param $machine_name
 */
function migrate_ui_deregister_migration($machine_name) {
  // The class is gone, so we'll manually clear migrate_status, and make
  // the default assumptions about the map/message tables.
  db_drop_table('migrate_map_' . strtolower($machine_name));
  db_drop_table('migrate_message_' . strtolower($machine_name));
  db_delete('migrate_status')
    ->condition('machine_name', $machine_name)
    ->execute();
  db_delete('migrate_field_mapping')
    ->condition('machine_name', $machine_name)
    ->execute();
  drupal_set_message(t("Deregistered '!description' task",
    array('!description' => $machine_name)));
}

/**
 * Menu callback
 */
function migrate_ui_handlers() {
  drupal_set_title(t('Migrate handler configuration'));
  return drupal_get_form('migrate_ui_handlers_form');
}

/**
 * Form for reviewing migrations.
 */
function migrate_ui_handlers_form($form, &$form_state) {
  $build = array();

  $build['handlers'] = array(
    '#type' => 'fieldset',
    '#title' => t('Handler configuration'),
    '#description' => t('In some cases, such as when a field handler for a contributed module is
          implemented in both migrate_extras and the module itself, you may need to disable
          a particular handler. In this case, you may uncheck the undesired handler below.'),
  );

  $build['handlers']['destination'] = array(
    '#type' => 'fieldset',
    '#title' => t('Destination handlers'),
    '#collapsible' => TRUE,
  );

  $header = array(
    'module' => array('data' => t('Module')),
    'class' => array('data' => t('Class')),
    'types' => array('data' => t('Destination types handled')),
  );

  $disabled = unserialize(variable_get('migrate_disabled_handlers', serialize(array())));
  $class_list = _migrate_class_list('MigrateDestinationHandler');
  $rows = array();
  $default_values = array();
  foreach ($class_list as $class_name => $handler) {
    $row = array();
    $module = db_select('registry', 'r')
              ->fields('r', array('module'))
              ->condition('name', $class_name)
              ->condition('type', 'class')
              ->execute()
              ->fetchField();
    $row['module'] = $module;
    $row['class'] = $class_name;
    $row['types'] = implode(', ', $handler->getTypesHandled());
    $default_values[$class_name] = !in_array($class_name, $disabled);
    $rows[$class_name] = $row;
  }
  $build['handlers']['destination']['destination_handlers'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $rows,
    '#default_value' => $default_values,
    '#empty' => t('No destination handlers found'),
  );

  $build['handlers']['field'] = array(
    '#type' => 'fieldset',
    '#title' => t('Field handlers'),
    '#collapsible' => TRUE,
  );

  $header = array(
    'module' => array('data' => t('Module')),
    'class' => array('data' => t('Class')),
    'types' => array('data' => t('Field types handled')),
  );

  $class_list = _migrate_class_list('MigrateFieldHandler');
  $rows = array();
  $default_values = array();
  foreach ($class_list as $class_name => $handler) {
    $row = array();
    $module = db_select('registry', 'r')
              ->fields('r', array('module'))
              ->condition('name', $class_name)
              ->condition('type', 'class')
              ->execute()
              ->fetchField();
    $row['module'] = $module;
    $row['class'] = $class_name;
    $row['types'] = implode(', ', $handler->getTypesHandled());
    $default_values[$class_name] = !in_array($class_name, $disabled);
    $rows[$class_name] = $row;
  }
  $build['handlers']['field']['field_handlers'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $rows,
    '#default_value' => $default_values,
    '#empty' => t('No field handlers found'),
  );

  $build['handlers']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save handler statuses'),
    '#submit' => array('migrate_ui_handlers_submit'),
  );

  return $build;
}

/**
 * Submit callback for the configuration form handler fieldset.
 */
function migrate_ui_handlers_submit($form, &$form_state) {
  $disabled = array();
  foreach ($form_state['values']['destination_handlers'] as $class => $value) {
    if (!$value) {
      $disabled[] = $class;
    }
  }
  foreach ($form_state['values']['field_handlers'] as $class => $value) {
    if (!$value) {
      $disabled[] = $class;
    }
  }
  variable_set('migrate_disabled_handlers', serialize($disabled));
  if (!empty($disabled)) {
    drupal_set_message(t('The following handler classes are disabled: @classes',
      array('@classes' => implode(', ', $disabled))));
  }
  else {
    drupal_set_message(t('No handler classes are currently disabled.'));
  }
}
