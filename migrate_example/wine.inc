<?php
// $Id$

/**
 * @file
 * Advanced migration examples. These serve two purposes:
 *
 * 1. To demonstrate some of the more advanced usages of the Migrate module.
 *    Search for "TIP:" below for features not found in the basic example.
 * 2. To provide thorough test cases for the simpletest suite.
 *
 */

/**
 * Abstract intermediate class holding common settings.
 */
abstract class AdvancedExampleMigration extends Migration {
  public function __construct() {
    // Always call the parent constructor first for basic setup
    parent::__construct();

    // With migrate_ui enabled, migration pages will indicate people involved in
    // the particular migration, with their role and contact info. We default the
    // list in the shared class; it can be overridden for specific migrations.
    $this->team = array(
      new MigrateTeamMember('Liz Taster', 'ltaster@example.com', t('Product Owner')),
      new MigrateTeamMember('Larry Brewer', 'lbrewer@example.com', t('Implementor')),
    );

    // Individual mappings in a migration can be linked to a ticket or issue
    // in an external tracking system. Define the URL pattern here in the shared
    // class with ':id:' representing the position of the issue number, then add
    // ->issueNumber(1234) to a mapping.
    $this->issuePattern = 'http://drupal.org/node/:id:';
  }
}

/**
 * TIP: While usually you'll create true migrations - processes that copy data
 * from some source into Drupal - you can also define processing steps for either
 * the import or rollback stages that take other actions. In this case, we want
 * to disable pathauto while the migration steps run.
 */
class WinePrepMigration extends MigrationBase {
  public function __construct() {
    parent::__construct();
    $this->description = t('If pathauto is present, disable it for the duration');
    // TIP: Regular dependencies, besides enforcing (in the absence of --force)
    // the run order of migrations, affect the sorting of migrations on display.
    // You can use soft dependencies to affect the display order when the
    // migrations aren't technically required to run in a certain order. In this
    // case, we want the wine migrations to appear after the beer migrations -
    // without this line, they would be intermingled due to their lack of
    // (formal) interdependencies.
    $this->softDependencies = array('BeerCommentMigration');
  }
  // Define isComplete(), returning a boolean, to indicate whether dependent
  // migrations may proceed
  public function isComplete() {
    // If pathauto is disabled, other migrations are free to go
    if (module_exists('pathauto')) {
      return FALSE;
    }
    else {
      return TRUE;
    }
  }
  // Implement any action you want to occur during an import process in an
  // import() process (alternatively, if you have an action which you want to
  // run during rollbacks, define a rollback() method).
  public function import() {
    if (module_exists('pathauto')) {
      module_disable(array('pathauto'));
    }
    // Provide feedback
    call_user_func($this->outputFunction, t("Succeeded in !sec sec - done with '!name'",
      array('!sec' => round(microtime(TRUE) - $this->starttime, 1),
            '!name' => $this->machineName)), 'completed');
    return Migration::RESULT_COMPLETED;
  }
}

class WineTermMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->dependencies = array('WinePrepMigration');
    $this->description = t('Migrate styles from the source database to taxonomy terms');
    $this->map = new MigrateSQLMap($this->machineName,
        array(
          'style' => array('type' => 'varchar',
                           'length' => 255,
                           'not null' => TRUE,
                           'description' => 'Topic ID',
                          )
        ),
        MigrateDestinationTerm::getKeySchema()
      );

    $query = db_select('migrate_example_wine_topic', 'met')
             ->fields('met', array('style', 'details', 'style_parent', 'region', 'hoppiness'))
             // This sort assures that parents are saved before children.
             ->orderBy('style_parent', 'ASC');
    $this->source = new MigrateSourceSQL($this, $query);
    $this->destination = new MigrateDestinationTerm('migrate_example_wine_styles');
    $this->addFieldMapping('name', 'style');
    $this->addFieldMapping('description', 'details');
    $this->addFieldMapping('parent_name', 'style_parent')
         ->description(t('The incoming style_parent field is the name of the term parent'));
    $this->addFieldMapping(NULL, 'hoppiness')
         ->description(t('This info will not be maintained in Drupal'))
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'region')
         ->description('Will a field be added to the vocabulary for this?')
         ->issueGroup(t('Client Issues'))
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM)
         ->issueNumber(770064);
    $this->addFieldMapping('format')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('weight')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('parent')
         ->issueGroup(t('DNM'));
  }
}

class WineUserMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Wine Drinkers of the world');
    $this->dependencies = array('WinePrepMigration');
    $this->map = new MigrateSQLMap($this->machineName,
        array('accountid' => array(
                'type' => 'int',
                'not null' => TRUE,
                'description' => 'Account ID.'
                )
             ),
        MigrateDestinationUser::getKeySchema()
    );
    $query = db_select('migrate_example_wine_account', 'mea')
             ->fields('mea', array('accountid', 'status', 'posted', 'name', 'nickname', 'password', 'mail'));
    $this->source = new MigrateSourceSQL($this, $query);
    $this->destination = new MigrateDestinationUser();

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('status', 'status');
    $this->addFieldMapping('created', 'posted')
         ->description('See prepare method');
    date_default_timezone_set('US/Mountain');
    $this->addFieldMapping('mail', 'mail');
    $this->addFieldMapping('pass', 'password');
    $this->addFieldMapping('roles')
         ->defaultValue(drupal_map_assoc(array(2)));

    // Unmapped source fields
    $this->addFieldMapping(NULL, 'nickname')
         ->issueGroup(t('DNM'));

    // Unmapped destination fields
    $this->addFieldMapping('theme')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('signature')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('access')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('login')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('timezone')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('picture')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('init')
         ->issueGroup(t('DNM'));
  }

  public function prepare(stdClass $account, stdClass $row) {
    // Source dates are in ISO format.
    // Because the mappings above have been applied, $account->created contains
    // the date/time string now - we could also pass $row->posted here.
    $account->created = strtotime($account->created);
  }
}

class WineNodeMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Wines of the world');
    $this->dependencies = array('WineTermMigration', 'WineUserMigration');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'bid' => array(
          'type' => 'int',
          'not null' => TRUE,
          'description' => 'Wine ID.',
          'alias' => 'b',
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    $query = db_select('migrate_example_wine_node', 'b')
             ->fields('b', array('bid', 'name', 'body', 'excerpt', 'accountid', 'image'));
    $query->leftJoin('migrate_example_wine_topic_node', 'tb', 'b.bid = tb.bid');
    // Gives a single comma-separated list of related terms
    $query->groupBy('tb.bid');
    $query->addExpression('GROUP_CONCAT(tb.style)', 'terms');
    $count_query = db_select('migrate_example_wine_node', 'b');
    $count_query->addExpression('COUNT(bid)', 'cnt');

    $this->source = new MigrateSourceSQL($this, $query, $count_query);
    $this->destination = new MigrateDestinationNode('migrate_example_wine');

    // Mapped fields
    $this->addFieldMapping('title', 'name')
         ->description(t('Mapping wine name in source to node title'));
    $this->addFieldMapping('nid', 'bid')
         ->description(t('Preserve old wine ID as nid in Drupal'));
    $this->addFieldMapping('is_new')
         ->defaultValue(TRUE);
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUserMigration')
         ->defaultValue(1);
    $this->addFieldMapping('migrate_example_wine_styles', 'terms')
         ->separator(',');
    $arguments = MigrateTextFieldHandler::arguments(array('source_field' => 'excerpt'));
    $this->addFieldMapping('body', 'body')
         ->arguments($arguments);
    $this->addFieldMapping(NULL, 'excerpt');
    $arguments = MigrateFileFieldHandler::arguments(drupal_get_path('module', 'migrate_example'), 'file_copy', FILE_EXISTS_RENAME);
    $this->addFieldMapping('field_migrate_example_image', 'image')
         ->arguments($arguments);
    $this->addFieldMapping('sticky')
         ->defaultValue(0);

    // No unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('name')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('created')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('changed')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('status')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('promote')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('revision')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
  }
}

class WineCommentMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = 'Comments about wines';
    $this->dependencies = array('WineUserMigration', 'WineNodeMigration');
    $this->map = new MigrateSQLMap($this->machineName,
      array('cid' => array(
              'type' => 'int',
              'not null' => TRUE,
             )
           ),
        MigrateDestinationComment::getKeySchema()
      );
    $query = db_select('migrate_example_wine_comment', 'mec')
             ->fields('mec', array('cid', 'cid_parent', 'name', 'mail', 'accountid', 'body', 'bid', 'subject'))
             ->orderBy('cid_parent', 'ASC');
    $this->source = new MigrateSourceSQL($this, $query);
    $this->destination = new MigrateDestinationComment('comment_node_migrate_example_wine');

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('subject', 'subject');
    $this->addFieldMapping('mail', 'mail');
    $this->addFieldMapping('status')
         ->defaultValue(COMMENT_PUBLISHED);
    $this->addFieldMapping('nid', 'bid');
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUserMigration')
         ->defaultValue(0);
    $this->addFieldMapping('pid', 'cid_parent')
         ->sourceMigration('WineCommentMigration')
         ->description('Parent comment.');

    $this->addFieldMapping('comment_body', 'body');

    // No unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('user_name')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('user_email')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('hostname')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('created')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('changed')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('thread')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('homepage')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
  }
}

class WineFinishMigration extends MigrationBase {
  public function __construct() {
    parent::__construct();
    $this->description = t('If pathauto is present and was previously enabled,
      re-enable it');
    $this->dependencies = array('WineCommentMigration');
  }
  public function isComplete() {
    if (module_exists('pathauto')) {
      return TRUE;
    }
    else {
      return FALSE;
    }
  }
  public function import() {
    if (!module_exists('pathauto')) {
      module_enable(array('pathauto'));
    }
    call_user_func($this->outputFunction,
      t("Succeeded in !sec sec - done with '!name'",
        array('!sec' => round(microtime(TRUE) - $this->starttime, 1),
              '!name' => $this->machineName)), 'completed');
    return Migration::RESULT_COMPLETED;
  }
}
