<?php
// $Id$

/**
 * @file
 * Support for user destinations.
 */

// TODO:
// Make sure this works with updates, explicit destination keys
// Speed up password generation a ton: $conf['password_count_log2'] = 1;

/**
 * Destination class implementing migration into users.
 */
class MigrateDestinationUser extends MigrateDestinationEntity {
  static public function getKeySchema() {
    return array(
      'uid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'ID of destination user',
      ),
    );
  }

  /**
   * Basic initialization
   */
  public function __construct($language = NULL, $text_format = NULL) {
    parent::__construct('user', 'user', $language, $text_format);
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
    $fields['roles'] = t('User: Roles');
    $fields['theme'] = t('User: Default theme');
    $fields['signature'] = t('User: Signature');
    $fields['signature_format'] = t('User: Signature format');
    $fields['created'] = t('User: Registered timestamp');
    $fields['access'] = t('User: Last access timestamp');
    $fields['login'] = t('User: Last login timestamp');
    $fields['status'] = t('User: Status');
    $fields['timezone'] = t('User: Timezone');
    $fields['language'] = t('User: Language');
    $fields['picture'] = t('User: Picture');
    $fields['init'] = t('User: Init');

    // Then add in anything provided by handlers
    $fields += migrate_handler_invoke_all('User', 'fields');

    return $fields;
  }

  /**
   * Delete a batch of users at once.
   *
   * @param $migration
   *  Controlling migration object. Unused here.
   * @param $uids
   *  Array of user IDs to be deleted.
   */
  public function bulkRollback(Migration $migration, array $uids) {
    migrate_instrument_start('user_delete_multiple');
    foreach ($uids as $uid) {
      user_delete(array(), $uid);
    }
    migrate_instrument_stop('user_delete_multiple');
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
  public function import(Migration $migration, stdClass $account, stdClass $row) {
    // Handle update case
    if (isset($row->migrate_map_destid1)) {
      $account->uid = $row->migrate_map_destid1;
    }

    $this->prepare($migration, $account, $row);
    migrate_instrument_start('user_save');
    $newaccount = user_save($account, (array)$account);
    migrate_instrument_stop('user_save');
    if ($newaccount) {
      $this->complete($migration, $newaccount, $row);
      $return = isset($newaccount->uid) ? array($newaccount->uid) : FALSE;
    }
    else {
      $return = FALSE;
    }
    return $return;
  }
}
