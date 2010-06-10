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
    parent::__construct();
    $this->team = array(
      new MigrateTeamMember('Jack Kramer', 'jkramer@example.com', t('Taster')),
      new MigrateTeamMember('Linda Madison', 'lmadison@example.com', t('Winemaker')),
    );
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
    // You can use soft dependencies to affect just the display order when the
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
      call_user_func($this->outputFunction, t('Disabled pathauto module'));
    }
    else {
      call_user_func($this->outputFunction, t('Pathauto is already disabled'));
    }
    // Must return one of the MigrationBase RESULT constants
    return MigrationBase::RESULT_COMPLETED;
  }
}

// The term migrations are very similar - implement the commonalities here
abstract class WineTermMigration extends AdvancedExampleMigration {
  public function __construct($type, $vocabulary_name, $description) {
    parent::__construct();
    $this->description = $description;
    $this->dependencies = array('WinePrepMigration');
    $this->map = new MigrateSQLMap($this->machineName,
        array(
          'categoryid' => array('type' => 'int',
                                'unsigned' => TRUE,
                                'not null' => TRUE,
                               )
        ),
        MigrateDestinationTerm::getKeySchema()
      );

    $query = db_select('migrate_example_wine_categories', 'wc')
             ->fields('wc', array('categoryid', 'name', 'details', 'category_parent'))
             ->condition('type', $type)
             // This sort assures that parents are saved before children.
             ->orderBy('category_parent', 'ASC');
    $this->source = new MigrateSourceSQL($this, $query);
    $this->destination = new MigrateDestinationTerm($vocabulary_name);

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('description', 'details');
    $this->addFieldMapping('parent', 'category_parent')
         ->sourceMigration(get_class($this));

    // Unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('format')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('weight')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('parent_name')
         ->issueGroup(t('DNM'));
  }
}

class WineVarietyMigration extends WineTermMigration {
  public function __construct() {
    parent::__construct('variety', 'migrate_example_wine_varieties',
      t('Migrate varieties from the source database to taxonomy terms'));
  }
}

class WineRegionMigration extends WineTermMigration {
  public function __construct() {
    parent::__construct('region', 'migrate_example_wine_regions',
      t('Migrate regions from the source database to taxonomy terms'));
  }
}

class WineBestWithMigration extends WineTermMigration {
  public function __construct() {
    parent::__construct('best_with', 'migrate_example_wine_best_with',
      t('Migrate "Best With" from the source database to taxonomy terms'));
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
                'unsigned' => TRUE,
                'not null' => TRUE,
                'description' => 'Account ID.'
                )
             ),
        MigrateDestinationUser::getKeySchema()
    );
    $query = db_select('migrate_example_wine_account', 'wa')
             ->fields('wa', array('accountid', 'status', 'posted', 'name', 'nickname', 'password', 'mail'));
    $this->source = new MigrateSourceSQL($this, $query);
    $this->destination = new MigrateDestinationUser();

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('status', 'status');
    $this->addFieldMapping('created', 'posted')
         ->description('See prepare method');
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
    // No genders for wine user
    $this->addFieldMapping('field_migrate_example_gender')
         ->issueGroup(t('DNM'));
  }

  public function prepare(stdClass $account, stdClass $row) {
    // Source dates are in ISO format.
    // Because the mappings above have been applied, $account->created contains
    // the date/time string now - we could also pass $row->posted here.
    $account->created = strtotime($account->created);
  }
}

class WineProducerMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Wine producers of the world');
    $this->dependencies = array('WineRegionMigration', 'WineUserMigration');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'producerid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'alias' => 'p',
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    $query = db_select('migrate_example_wine_producer', 'p')
             ->fields('p', array('producerid', 'name', 'body', 'excerpt', 'accountid'));
    // Region term is singletons, handled straighforwardly
    $query->leftJoin('migrate_example_wine_category_producer', 'reg',
      "p.producerid = reg.producerid");
    $query->addField('reg', 'categoryid', 'region');

    $this->source = new MigrateSourceSQL($this, $query);
    $this->destination = new MigrateDestinationNode('migrate_example_producer');

    // Mapped fields
    $this->addFieldMapping('title', 'name')
         ->description(t('Mapping producer name in source to node title'));
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUserMigration')
         ->defaultValue(1);
    $this->addFieldMapping('migrate_example_wine_regions', 'region')
         ->sourceMigration('WineRegionMigration');
    $arguments = MigrateTextFieldHandler::arguments(array('source_field' => 'excerpt'));
    $this->addFieldMapping('body', 'body')
         ->arguments($arguments);
    $this->addFieldMapping(NULL, 'excerpt');
    $this->addFieldMapping('sticky')
         ->defaultValue(0);

    // No unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('is_new')
         ->issueGroup(t('DNM'));
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

// TODO: Add node_reference field pointing to producer
class WineWineMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Wines of the world');
    $this->dependencies = array('WineVarietyMigration', 'WineRegionMigration',
      'WineBestWithMigration', 'WineUserMigration', 'WineProducerMigration');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'wineid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'description' => 'Wine ID',
          'alias' => 'w',
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    $query = db_select('migrate_example_wine', 'w')
             ->fields('w', array('wineid', 'name', 'body', 'excerpt', 'accountid', 'image'));
    // Variety and region terms are singletons, need to go two levels deep
    $query->leftJoin('migrate_example_wine_category_wine', 'cwvar',
      "w.wineid = cwvar.wineid");
    $query->leftJoin('migrate_example_wine_categories', 'var',
      "cwvar.categoryid = var.categoryid AND var.type = 'variety'");
    $query->addField('var', 'categoryid', 'variety');

    $query->leftJoin('migrate_example_wine_category_wine', 'cwreg',
      "w.wineid = cwreg.wineid");
    $query->leftJoin('migrate_example_wine_categories', 'reg',
      "cwreg.categoryid = reg.categoryid AND var.type = 'region'");
    $query->addField('reg', 'categoryid', 'region');

    $query->leftJoin('migrate_example_wine_category_wine', 'cwbw',
      "w.wineid = cwbw.wineid");
    $query->leftJoin('migrate_example_wine_categories', 'bw',
      "cwbw.categoryid = bw.categoryid AND bw.type = 'best_with'");
    // Gives a single comma-separated list of related terms
    $query->groupBy('cwbw.wineid');
    $query->addExpression('GROUP_CONCAT(bw.categoryid)', 'best_with');

    $count_query = db_select('migrate_example_wine', 'w');
    $count_query->addExpression('COUNT(wineid)', 'cnt');

    $this->source = new MigrateSourceSQL($this, $query, $count_query);
    $this->destination = new MigrateDestinationNode('migrate_example_wine');

    // Mapped fields
    $this->addFieldMapping('title', 'name')
         ->description(t('Mapping wine name in source to node title'));
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUserMigration')
         ->defaultValue(1);
    $this->addFieldMapping('migrate_example_wine_varieties', 'variety')
         ->sourceMigration('WineVarietyMigration');
    $this->addFieldMapping('migrate_example_wine_regions', 'region')
         ->sourceMigration('WineRegionMigration');
    $this->addFieldMapping('migrate_example_wine_best_with', 'best_with')
         ->separator(',')
         ->sourceMigration('WineBestWithMigration');
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
    $this->addFieldMapping('is_new')
         ->issueGroup(t('DNM'));
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
    $this->dependencies = array('WineUserMigration', 'WineWineMigration');
    $this->map = new MigrateSQLMap($this->machineName,
      array('commentid' => array(
              'type' => 'int',
              'unsigned' => TRUE,
              'not null' => TRUE,
             )
           ),
        MigrateDestinationComment::getKeySchema()
      );
    $query = db_select('migrate_example_wine_comment', 'wc')
             ->fields('wc', array('commentid', 'comment_parent', 'name', 'mail', 'accountid', 'body', 'wineid', 'subject'))
             ->orderBy('comment_parent');
    $this->source = new MigrateSourceSQL($this, $query);
    $this->destination = new MigrateDestinationComment('comment_node_migrate_example_wine');

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('subject', 'subject');
    $this->addFieldMapping('mail', 'mail');
    $this->addFieldMapping('status')
         ->defaultValue(COMMENT_PUBLISHED);
    $this->addFieldMapping('nid', 'wineid');
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUserMigration')
         ->defaultValue(0);
    $this->addFieldMapping('pid', 'comment_parent')
         ->sourceMigration('WineCommentMigration')
         ->description('Parent comment');

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
      // TODO: Really should keep track of whether it was originally enabled
      module_enable(array('pathauto'));
      call_user_func($this->outputFunction, t('Re-enabled pathauto module'));
    }
    else {
      call_user_func($this->outputFunction, t('Pathauto module already enabled'));
    }
    return Migration::RESULT_COMPLETED;
  }
}
