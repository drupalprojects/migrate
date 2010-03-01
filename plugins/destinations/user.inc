<?php
// $Id$

/**
 * @file
 * Support for user destinations.
 */

// TODO:
// Make sure this works with updates, explicit destination keys

/**
 * Destination class implementing migration into users.
 */
class MigrateDestinationUser extends MigrateDestinationEntity {
  /**
   * Basic initialization
   */
  public function __construct($language = LANGUAGE_NONE, $input_format = NULL) {
    // PK definition, for use in constructing map/message tables
    $destination_key = array(
      'uid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'ID of destination user',
      ),
    );
    parent::__construct($destination_key, 'user', 'user', $language, $input_format);
  }

  /**
   * Returns a list of fields available to be mapped for users
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    $fields = array();
    // First the core (users table) properties
    $fields['uid'] = t('User: Existing user ID');
    $fields['name'] = t('User: Username');
    $fields['pass'] = t('User: Password (plain text)');
    $fields['mail'] = t('User: Email address');
    $fields['theme'] = t('User: Default theme');
    $fields['signature'] = t('User: Signature');
    $fields['created'] = t('User: Registered timestamp');
    $fields['access'] = t('User: Last access timestamp');
    $fields['login'] = t('User: Last login timestamp');
    $fields['status'] = t('User: Status');
    $fields['timezone'] = t('User: Timezone');
    $fields['language'] = t('User: Language');
    $fields['picture'] = t('User: Picture');

    // Then add any fields attached to users
    $field_instance_info = field_info_instances('user', 'user');
    foreach ($field_instance_info as $machine_name => $instance) {
      $field_info = field_info_field($machine_name);

      $fields[$machine_name] = t('User:') . ' ' . $instance['label'] .
        ' (' . $field_info['type'] . ')';
    }
    // TODO: Then add in anything provided by handlers

    return $fields;
  }

  /**
   * Delete a migrated user
   *
   * @param $migration
   *  Controlling migration object. Unused here.
   * @param $key
   *  Array of fields representing the key (in this case, just uid).
   */
  public function rollback(Migration $migration, array $key) {
    // TODO: the user cancel api is not very friendly to programmatic deletion. See http://drupal.org/node/705306
    // Avoid batch API for now.
    $edit = array();
    migrate_instrument_start('user_load');
    $account = user_load($key[0], TRUE);
    migrate_instrument_stop('user_load');
    $method = 'user_cancel_delete';
    migrate_instrument_start('user_cancel hooks');
    module_invoke_all('user_cancel', $edit, $account, $method);
    migrate_instrument_stop('user_cancel hooks');
    migrate_instrument_start('_user_cancel');
    _user_cancel(array(), $account, $method); // Slow. Clears cache.
    migrate_instrument_stop('_user_cancel');
    return (bool) $account->uid;
  }

  /**
   * Import a single user.
   *
   * @param $migration
   *  Controlling migration object. Passed through to prepare/complete handlers.
   * @param $account
   *  Account object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields (uid only in this case) of the user that was saved if
   *  successful. FALSE on failure.
   */
  public function import(Migration $migration, stdClass $account, stdClass $row, array $arguments) {
    // Look for Field API fields and handle appropriately
    $this->prepareFields($account, $arguments);
    migrate_handler_invoke_all('User', 'prepare', $migration, $account, $row);
    migrate_instrument_start('user_save');
    $newaccount = user_save($account, (array)$account);
    migrate_instrument_stop('user_save');
    migrate_handler_invoke_all('User', 'complete', $migration, $account, $row);
    $return = isset($newaccount->uid) ? array($newaccount->uid) : FALSE;
    return $return;
  }
}
