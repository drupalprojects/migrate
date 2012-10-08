<?php

/**
 * @file
 * Import wizard framework, based on the form_example wizard from
 * http://drupal.org/project/examples.
 */

/**
 * The primary formbuilder function for the wizard form.
 *
 * This form has two defined submit handlers to process the different steps:
 *  - Previous: handles the way to get back one step in the wizard.
 *  - Next:     handles each step form submission,
 *
 * The third handler, the finish button handler, is the default form _submit
 * handler used to process the information.
 */
function migrate_ui_wizard($form, &$form_state, $machine_name = '') {
  // Initialize the step information our first time in.
  if (empty($form_state['step'])) {
    $form_state['step'] = 0;
    $wizards = migrate_ui_wizards();
    $wizard = $wizards[$machine_name];
    $form_state['wizard'] = $wizard;

    // This array contains the function to be called at each step to get the
    // relevant form elements. It will also store state information for each
    // step. We insert a 0th step as an introduction, and to gather a unique
    // group name.
    $step_0 = array(0 => 'migrate_ui_wizard_overview');
    $form_state['step_information'] = $step_0 + $wizard['steps'];
    // Make sure validate/submit handlers can be called
    $form_state['build_info']['files']['menu'] = $wizard['filename'];

    $form_state['storage']['migrations'] = array();
  }
  else {
    $wizard = $form_state['wizard'];
  }

  $step = &$form_state['step'];

  if ($step == 0) {
    drupal_set_title(t('Import from @source_title: Overview',
      array('@source_title' => $wizard['source_title'])));
  }
  else {
    drupal_set_title(t('Import from @source_title, step @step: @step_name',
      array('@source_title' => $wizard['source_title'], '@step' => $step,
        '@step_name' => $form_state['step_information'][$step]['name'])));
  }

  // Call the function named in $form_state['step_information'] to get the
  // form elements to display for this step.
  $form = $form_state['step_information'][$step]($form, $form_state);

  // Show the 'previous' button if appropriate. Note that #submit is set to
  // a special submit handler, and that we use #limit_validation_errors to
  // skip all complaints about validation when using the back button. The
  // values entered will be discarded, but they will not be validated, which
  // would be annoying in a "back" button.
  if ($step > 0) {
    $form['prev'] = array(
      '#type' => 'submit',
      '#value' => t('Previous'),
      '#name' => 'prev',
      '#submit' => array('migrate_ui_wizard_previous_submit'),
      '#limit_validation_errors' => array(),
    );
  }

  // Show the Next button only if there are more steps defined.
  if ($step < count($form_state['step_information']) - 1) {
    $form['next'] = array(
      '#type' => 'submit',
      '#value' => t('Next'),
      '#name' => 'next',
      '#submit' => array('migrate_ui_wizard_next_submit'),
    );
  }
  else {
    // Just in case there are no more steps, we use the default submit handler of
    // the form wizard.
    $form['finish'] = array(
      '#type' => 'submit',
      '#value' => t('Finish'),
    );
  }

  // Include each validation function defined for the different steps.
  if (function_exists($form_state['step_information'][$step]['form'] . '_validate')) {
    $form['next']['#validate'] = array($form_state['step_information'][$step]['form'] . '_validate');
  }

  return $form;
}

/**
 * Submit handler for the "previous" button.
 * - Stores away $form_state['values']
 * - Decrements the step counter
 * - Replaces $form_state['values'] with the values from the previous state.
 * - Forces form rebuild.
 */
function migrate_ui_wizard_previous_submit($form, &$form_state) {
  $current_step = &$form_state['step'];
  $form_state['step_information'][$current_step]['stored_values'] = $form_state['values'];
  if ($current_step > 0) {
    $current_step--;
    $form_state['values'] = $form_state['step_information'][$current_step]['stored_values'];
  }
  $form_state['rebuild'] = TRUE;
}

/**
 * Submit handler for the 'next' button.
 * - Saves away $form_state['values']
 * - Increments the step count.
 * - Replace $form_state['values'] from the last time we were at this page
 *   or with array() if we haven't been here before.
 * - Force form rebuild.
 *
 * @param array $form
 * @param array $form_state
 */
function migrate_ui_wizard_next_submit($form, &$form_state) {
  $current_step = &$form_state['step'];
  $form_state['step_information'][$current_step]['stored_values'] = $form_state['values'];

  if ($current_step < count($form_state['step_information'])) {
    $current_step++;
    if (!empty($form_state['step_information'][$current_step]['stored_values'])) {
      $form_state['values'] = $form_state['step_information'][$current_step]['stored_values'];
    }
    else {
      $form_state['values'] = array();
    }
    $form_state['rebuild'] = TRUE;  // Force rebuild with next step.
  }
}

/**
 * Wizard form submit handler.
 *
 * @param array $form
 * @param array $form_state
 */
function migrate_ui_wizard_submit($form, &$form_state) {
  drupal_set_message("We're just a POC for now - something will actually happen later.");

  $form_state['redirect'] = 'admin/content/migrate';
  foreach ($form_state['storage']['migrations'] as $machine_name => $info) {
    MigrationBase::registerMigration($info['class_name'], $machine_name, $info['arguments']);
  }
return;
  try {
    $site = migrate_drupal_site($form_state['storage']['endpoint'], $form_state['storage']['http_options']['headers']['Cookie']);
  }
  catch (Exception $e) {
    drupal_set_message($e->getMessage());
    return;
  }
  return;
  // Register a Term migration for each vocabulary mapping
  $vocabs = $form_state['storage']['vocabs'];
  foreach ($vocabs as $source_vid => $bundle) {
    $arguments = array('endpoint' => $form_state['storage']['endpoint'],
      'source_vid' => $source_vid, 'bundle' => $bundle);
    Migration::registerMigration('DrupalTerm', "DrupalTerm$source_vid", $arguments);
  }
  return;
  foreach ($form_state['values']['vocab'] as $source_vid => $dest_vid) {
    // New vocabulary requested, retrieve data on old one
    if ($dest_vid == 0) {
      $url = $form_state['storage']['endpoint'] . '/taxonomy_vocabulary/' . $source_vid;
      $response = drupal_http_request($url, $form_state['storage']['http_options']);
      if ($response->code == '200') {
        $vocab = json_decode($response->data);
        // Lack of a machine_name indicates a D6 source, generate the machine_name
        if (!isset($vocab->machine_name)) {
          $name = drupal_strtolower($vocab->name);
          $vocab->machine_name = preg_replace('/[^a-z0-9]+/', '_', $name);
        }
        unset($vocab->vid);
        if (taxonomy_vocabulary_save($vocab)) {
          $form_state['storage']['vocabs'][$source_vid] = $vocab->machine_name;
          drupal_set_message(t('New vocabulary !name created', array('!name' => $vocab->name)));
        }
        else {
          form_set_error('vocab][' . $source_vid, t('Failed to create vocabulary !name',
            array('!name' => $vocab->name)));
        }
      }
      else {
        form_set_error('description', t('Could not connect to !url', array('!url' => $url)));
      }
    }
    elseif ($dest_vid != -1) {
      $vocab = taxonomy_vocabulary_load($dest_vid);
      $form_state['storage']['vocabs'][$source_vid] = $vocab->machine_name;
    }
  }
}

/**
 * Step 0 - tell 'em what we're doing, and get a unique group name/migration
 * prefix.
 *
 * @param array $form
 * @param aray $form_state
 *
 * @return array
 */
function migrate_ui_wizard_overview($form, &$form_state) {
  $form['overview'] = array(
    '#prefix' => '<p>',
    '#markup' => $form_state['wizard']['overview'],
    '#suffix' => '</p>',
  );
  $form['description'] = array(
    '#prefix' => '<p>',
    '#markup' => t('You will be led through a series of steps, allowing you to ' .
      'customize what will be imported into Drupal and how it will be mapped. ' .
      'At the end of this process, a set of import steps (called <em>Migrations</em>) '.
      'are generated and you will be left at the Migrate dashboard, from which ' .
      'you can perform the imports, monitor their progress, roll them back, etc.'),
    '#suffix' => '</p>',
  );
  $default_value = !empty($form_state['values']['group_name']) ?
    $form_state['values']['group_name'] : '';
  $form['group_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Unique name'),
    '#description' => t('Provide a unique alphanumeric name for this import. It ' .
      'will be used to prefix and group the generated migrations.'),
    '#size' => 16,
    '#maxlength' => 16,
    '#required' => TRUE,
    '#default_value' => $default_value,
  );
  return $form;
}

/**
 * Make sure the prefix is alphanumeric and unique.
 *
 * @param array $form
 * @param array $form_state
 */
function migrate_ui_wizard_overview_validate($form, &$form_state) {
  $group_name = trim($form_state['values']['group_name']);
  if (!preg_match('|^[[:alnum:]]+$|', $group_name)) {
    form_set_error('group_name',
      t('The unique name for your import, %name, must be alphanumeric.',
        array('%name' => $group_name)));
  }
  $lower_group_name = drupal_strtolower($group_name);
  $migrations = migrate_migrations();
  foreach ($migrations as $migration) {
    $group = drupal_strtolower($migration->getGroup()->getName());
    if ($lower_group_name == $group) {
      form_set_error('group_name',
        t('The name for your import, %name, must be unique. There is already a ' .
          'migration group with that name.',
          array('%name' => $group_name)));
      break;
    }
  }
  $form_state['storage']['group_name'] = $group_name;
}

function migrate_ui_wizard_add_migration(&$form_state, $machine_name,
    $class_name, $arguments) {
  $arguments += array(
    'group' => MigrateGroup::getInstance($form_state['storage']['group_name']),
    'machine_name' => $machine_name,
  );
  $form_state[$machine_name] = array(
    'class_name' => $class_name,
    'arguments' => $arguments,
  );
}

abstract class MigrateUIWizard {
  abstract static public function wizardConfiguration();
//  abstract public function credentials($form, &$form_state);
}
