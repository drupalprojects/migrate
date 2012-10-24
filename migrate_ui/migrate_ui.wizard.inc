<?php

/**
 * @file
 * Import wizard framework.
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
  if (empty($form_state['wizard'])) {
    $wizard = new $machine_name();
    $form_state['wizard'] = $wizard;
  }
  else {
    $wizard = $form_state['wizard'];
  }

  $form = $wizard->form();

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
  $wizard = $form_state['wizard'];
  $wizard->previousStep();
  $form_state['values'] = $wizard->currentStep->getFormValues();
  $form_state['rebuild'] = TRUE;
}

/**
 * Validate handler for the 'next' button.
 * - Saves away $form_state['values']
 * - Increments the step count.
 * - Replace $form_state['values'] from the last time we were at this page
 *   or with array() if we haven't been here before.
 * - Force form rebuild.
 *
 * @param array $form
 * @param array $form_state
 */
function migrate_ui_wizard_next_validate($form, &$form_state) {
  $wizard = $form_state['wizard'];
  $wizard->formValidate($form_state);
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
  $wizard = $form_state['wizard'];
  $wizard->currentStep->setFormValues($form_state['values']);
  $wizard->nextStep();
  if ($wizard->currentStep) {
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
  return;
  foreach ($form_state['storage']['migrations'] as $machine_name => $info) {
    MigrationBase::registerMigration($info['class_name'], $machine_name, $info['arguments']);
  }
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

abstract class MigrateUIWizard {
  public $firstStep = NULL;
  public $lastStep = NULL;
  public $currentStep = NULL;
  protected $stepNumber = 1;
  protected $groupName = 'default';
  protected $migrations = array();

  /**
   * Returns the translatable name representing the source of the data (e.g.,
   * "Drupal", "WordPress", etc.).
   *
   * @return string
   */
  abstract public function getSourceName();


  /**
   * Returns a translatable description of what this particular import module
   * does, requirements, etc., to be inserted into the initial (overview) form
   * of the wizard.
   *
   * @return string
   */
  abstract public function overviewDescription();

  public function __construct() {
    $this->addStep(t('Overview'), 'overviewForm');
  }

  protected function addStep($name, $form, MigrateUIStep $after = NULL) {
    $new_step = new MigrateUIStep($name, $form);
    if (is_null($after)) {
      $after = $this->lastStep;
    }
    if (is_null($after)) {
      $this->firstStep = $this->lastStep = $this->currentStep = $new_step;
    }
    else {
      $new_step->nextStep = $after->nextStep;
      if (is_null($new_step->nextStep)) {
        $this->lastStep = $new_step;
      }
      $new_step->previousStep = $after;
      $after->nextStep = $new_step;
    }
    return $new_step;
  }

  public function nextStep() {
    if ($this->currentStep && $this->currentStep->nextStep) {
      $this->currentStep = $this->currentStep->nextStep;
      $this->stepNumber++;
    }
  }

  public function previousStep() {
    if ($this->currentStep && $this->currentStep->previousStep) {
      $this->currentStep = $this->currentStep->previousStep;
      $this->stepNumber--;
    }
  }

  public function form() {
    drupal_set_title(t('Import from @source_title, step @step: @step_name',
      array('@source_title' => $this->getSourceName(), '@step' => $this->stepNumber,
        '@step_name' => $this->currentStep->getName())));

    $form_function = $this->currentStep->getForm();
    $form = $this->{$form_function}();

    $form['actions'] = array('#type' => 'actions');

    // Show the 'previous' button if appropriate. Note that #submit is set to
    // a special submit handler, and that we use #limit_validation_errors to
    // skip all complaints about validation when using the back button. The
    // values entered will be discarded, but they will not be validated, which
    // would be annoying in a "back" button.
    if ($this->currentStep != $this->firstStep) {
      $form['actions']['prev'] = array(
        '#type' => 'submit',
        '#value' => t('Previous'),
        '#name' => 'prev',
        '#submit' => array('migrate_ui_wizard_previous_submit'),
        '#limit_validation_errors' => array(),
      );
    }

    // Show the Next button only if there are more steps defined.
    if ($this->currentStep == $this->lastStep) {
      $form['actions']['finish'] = array(
        '#type' => 'submit',
        '#value' => t('Finish'),
      );
    }
    else {
      $form['actions']['next'] = array(
        '#type' => 'submit',
        '#value' => t('Next'),
        '#name' => 'next',
        '#submit' => array('migrate_ui_wizard_next_submit'),
        '#validate' => array('migrate_ui_wizard_next_validate'),
      );
    }
    return $form;
  }

  public function formValidate(&$form_state) {
    $validate_function = $this->currentStep->getFormValidate();
    $this->{$validate_function}($form_state);
  }

  protected function overviewForm() {
    $form['overview'] = array(
      '#prefix' => '<p>',
      '#markup' => $this->overviewDescription(),
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

    $form['group_name'] = array(
      '#type' => 'textfield',
      '#title' => t('Unique name'),
      '#description' => t('Provide a unique alphanumeric name for this import. It ' .
        'will be used to prefix and group the generated migrations.'),
      '#size' => 16,
      '#maxlength' => 16,
      '#required' => TRUE,
      '#default_value' => $this->groupName,
    );
    return $form;
  }

  /**
  * Make sure the prefix is alphanumeric and unique.
  *
  * @param array $form_state
  */
  protected function overviewFormValidate(&$form_state) {
    $this->groupName = trim($form_state['values']['group_name']);
    if (!preg_match('|^[[:alnum:]]+$|', $this->groupName)) {
      form_set_error('group_name',
        t('The unique name for your import, %name, must be alphanumeric.',
          array('%name' => $this->groupName)));
    }
    $lower_group_name = drupal_strtolower($this->groupName);
    $migrations = migrate_migrations();
    foreach ($migrations as $migration) {
      $group = drupal_strtolower($migration->getGroup()->getName());
      if ($lower_group_name == $group) {
        form_set_error('group_name',
          t('The name for your import, %name, must be unique. There is already a ' .
            'migration group with that name.',
            array('%name' => $this->groupName)));
        break;
      }
    }
  }

  abstract protected function sourceDataForm();
  abstract protected function sourceDataFormValidate(&$form_state);

  protected function addMigration($machine_name, $class_name, $arguments) {
    $arguments += array(
      'group' => $this->groupName,
      'machine_name' => $machine_name,
    );
    $this->migrations[$machine_name] = array(
      'class_name' => $class_name,
      'arguments' => $arguments,
    );
  }


}

class MigrateUIStep {
  protected $name;
  public function getName() {
    return $this->name;
  }
  protected $form;
  public function getForm() {
    return $this->form;
  }
  public function getFormValidate() {
    return $this->form . 'Validate';
  }
  protected $formValues;
  public function getFormValues() {
    return $this->formValues;
  }
  public function setFormValues($form_values) {
    $this->formValues = $form_values;
  }
  protected $data;
  public function getData() {
    return $this->data;
  }
  public function setData($data) {
    $this->data = $data;
  }

  public $nextStep = NULL;
  public $previousStep = NULL;

  public function __construct($name, $form) {
    $this->name = $name;
    $this->form = $form;
  }
}
