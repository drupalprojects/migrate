<?php

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
  public $basicFormat;

  public function __construct() {
    parent::__construct();
    $this->team = array(
      new MigrateTeamMember('Jack Kramer', 'jkramer@example.com', t('Taster')),
      new MigrateTeamMember('Linda Madison', 'lmadison@example.com', t('Winemaker')),
    );
    $this->issuePattern = 'http://drupal.org/node/:id:';

    // A format of our own, for testing migration of formats
    $this->basicFormat = filter_format_load('migrate_example');
  }
}

/**
 * TIP: While usually you'll create true migrations - processes that copy data
 * from some source into Drupal - you can also define processing steps for either
 * the import or rollback stages that take other actions. In this case, we want
 * to disable auto_nodetitle while the migration steps run.
 */
class WinePrepMigration extends MigrationBase {
  // Remember whether the auto_nodetitle was originally enabled, so we know whether
  // to re-enable it
  public static $wasEnabled = FALSE;

  public function __construct() {
    parent::__construct();
    $this->description = t('If auto_nodetitle is present, disable it for the duration');
    // TIP: Regular dependencies, besides enforcing (in the absence of --force)
    // the run order of migrations, affect the sorting of migrations on display.
    // You can use soft dependencies to affect just the display order when the
    // migrations aren't technically required to run in a certain order. In this
    // case, we want the wine migrations to appear after the beer migrations -
    // without this line, they would be intermingled due to their lack of
    // (formal) interdependencies.
    $this->softDependencies = array('BeerComment');
  }
  // Define isComplete(), returning a boolean, to indicate whether dependent
  // migrations may proceed
  public function isComplete() {
    // If Auto Node Title is disabled, other migrations are free to go
    if (module_exists('auto_nodetitle')) {
      return FALSE;
    }
    else {
      return TRUE;
    }
  }
  // Implement any action you want to occur during an import process in an
  // import() method (alternatively, if you have an action which you want to
  // run during rollbacks, define a rollback() method).
  public function import() {
    if (module_exists('auto_nodetitle')) {
      self::$wasEnabled = TRUE;
      module_disable(array('auto_nodetitle'));
      $this->showMessage(t('Disabled auto_nodetitle module'), 'success');
    }
    else {
      self::$wasEnabled = FALSE;
      $this->showMessage(t('Auto_nodetitle is already disabled'), 'success');
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
    $this->dependencies = array('WinePrep');
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
             ->fields('wc', array('categoryid', 'name', 'details', 'category_parent', 'ordering'))
             ->condition('type', $type)
             // This sort assures that parents are saved before children.
             ->orderBy('category_parent', 'ASC');
    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationTerm($vocabulary_name);

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('description', 'details');
    $this->addFieldMapping('parent', 'category_parent')
         ->sourceMigration($this->getMachineName());
    $this->addFieldMapping('weight', 'ordering');
    $this->addFieldMapping('format')
         ->defaultValue($this->basicFormat->format);

    // Unmapped source fields

    // Unmapped destination fields
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
    $this->dependencies = array('WinePrep');
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
             ->fields('wa', array('accountid', 'status', 'posted', 'name',
                'password', 'mail', 'last_access', 'last_login',
                'original_mail', 'sig', 'sex'));
    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationUser();

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('status', 'status');
    $this->addFieldMapping('created', 'posted')
         ->description('See prepare method');
    $this->addFieldMapping('access', 'last_access')
         ->description('See prepare method');
    $this->addFieldMapping('login', 'last_login')
         ->description('See prepare method');
    $this->addFieldMapping('mail', 'mail');
    $this->addFieldMapping('pass', 'password');
    $this->addFieldMapping('roles')
         ->defaultValue(drupal_map_assoc(array(2)));
    $this->addFieldMapping('signature', 'sig');
    $this->addFieldMapping('signature_format')
         ->defaultValue($this->basicFormat->format);
    $this->addFieldMapping('init', 'original_mail');
    $this->addFieldMapping('field_migrate_example_gender', 'sex')
         ->description(t('Map from M/F to 0/1 in prepare method'));

    // Unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('theme')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('timezone')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('picture')
         ->issueGroup(t('DNM'));
  }

  public function prepare(stdClass $account, stdClass $row) {
    // Source dates are in ISO format.
    // Because the mappings above have been applied, $account->created contains
    // the date/time string now - we could also pass $row->posted here.
    $account->created = strtotime($account->created);
    $account->access = strtotime($account->access);
    $account->login = strtotime($account->login);

    // Gender data comes in as M/F, needs to be saved as Male=0/Female=1
    // TIP: Note that the Migration prepare method is called after all other
    // prepare handlers. Most notably, the field handlers have had their way
    // and created field arrays, so we have to save in the same format.
    switch ($row->sex) {
      case 'm':
      case 'M':
        $account->field_migrate_example_gender[LANGUAGE_NONE][0]['value'] = 0;
        break;
      case 'f':
      case 'F':
        $account->field_migrate_example_gender[LANGUAGE_NONE][0]['value'] = 1;
        break;
      default:
        unset($account->field_migrate_example_gender);
        break;
    }
  }
}

class WineProducerMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Wine producers of the world');
    $this->dependencies = array('WineRegion', 'WineUser');

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

    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationNode('migrate_example_producer');

    // Mapped fields
    $this->addFieldMapping('title', 'name')
         ->description(t('Mapping producer name in source to node title'));
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUser')
         ->defaultValue(1);
    $this->addFieldMapping('migrate_example_wine_regions', 'region')
         ->sourceMigration('WineRegion')
         ->arguments(array('source_type' => 'tid'));
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

/**
 * TIP: An example of importing from an XML feed. See the files in the xml
 * directory - index.xml contains a list of IDs to import, and <id>.xml
 * is the data for a given producer.
 *
 * Note that, if basing a migration on an XML source, you need to derive it
 * from XMLMigration instead of Migration.
 */
class WineProducerXMLMigration extends XMLMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('XML feed of wine producers of the world');
    $this->dependencies = array('WineRegion', 'WineUser');

    // There isn't a consistent way to automatically identify appropriate "fields"
    // from an XML feed, so we pass an explicit list of source fields
    $fields = array(
      'name' => t('Producer name'),
      'description' => t('Description of producer'),
      'authorid' => t('Numeric ID of the author'),
      'region' => t('Name of region'),
    );

    // The source ID here is the one retrieved from the XML listing file, and
    // used to identify the specific item's file
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'sourceid' => array(
          'type' => 'varchar',
          'length' => 4,
          'not null' => TRUE,
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // This can also be an URL instead of a file path.
    $xml_folder = DRUPAL_ROOT . '/' . drupal_get_path('module', 'migrate_example') . '/xml/';
    $list_url = $xml_folder . 'index.xml';
    // Each ID retrieved from the list URL will be plugged into :id in the
    // item URL to fetch the specific objects.
    $item_url = $xml_folder . ':id.xml';

    // We use the MigrateSourceList class for any source where we obtain the list
    // of IDs to process separately from the data for each item. The listing
    // and item are represented by separate classes, so for example we could
    // replace the XML listing with a file directory listing, or the XML item
    // with a JSON item.
    $this->source = new MigrateSourceList(new MigrateListXML($list_url),
      new MigrateItemXML($item_url), $fields);

    $this->destination = new MigrateDestinationNode('migrate_example_producer');

    // TIP: Note that for XML sources, in addition to the source field passed to
    // addFieldMapping (the name under which it will be saved in the data row
    // passed through the migration process) we specify the Xpath used to retrieve
    // the value from the XML.
    $this->addFieldMapping('title', 'name')
         ->xpath('/producer/name');
    $this->addFieldMapping('uid', 'authorid')
         ->xpath('/producer/authorid')
         ->sourceMigration('WineUser')
         ->defaultValue(1);
    $this->addFieldMapping('migrate_example_wine_regions', 'region')
         ->xpath('/producer/region');
    $this->addFieldMapping('body', 'description')
         ->xpath('/producer/description');
  }
}

// TODO: Add node_reference field pointing to producer
class WineWineMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Wines of the world');
    $this->dependencies = array('WineVariety', 'WineRegion',
      'WineBestWith', 'WineUser', 'WineProducer');

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
             ->fields('w', array('wineid', 'name', 'body', 'excerpt', 'accountid',
              'image', 'posted', 'last_changed', 'variety', 'region', 'rating'));
    $query->leftJoin('migrate_example_wine_category_wine', 'cwbw',
      "w.wineid = cwbw.wineid");
    $query->leftJoin('migrate_example_wine_categories', 'bw',
      "cwbw.categoryid = bw.categoryid AND bw.type = 'best_with'");
    // Gives a single comma-separated list of related terms
    $query->groupBy('w.wineid');
    $query->addExpression('GROUP_CONCAT(bw.categoryid)', 'best_with');

    $count_query = db_select('migrate_example_wine', 'w');
    $count_query->addExpression('COUNT(wineid)', 'cnt');

    // TIP: By passing an array of source fields to the MigrateSourceSQL constructor,
    // we can modify the descriptions of source fields (which just default, for
    // SQL migrations, to table_alias.column_name), as well as add additional fields
    // (which may be populated in prepareRow()).
    $source_fields = array(
      'wineid' => t('Wine ID in the old system'),
      'name' => t('The name of the wine'),
      'best_vintages' => t('What years were best for this wine?'),
    );

    // TIP: By default, each time a migration is run, any previously unimported source items
    // are imported (along with any previously-imported items marked for update). If the
    // source data contains a timestamp that is set to the creation time of each new item,
    // as well as set to the update time for any existing items that are updated, then
    // you can have those updated items automatically reimported by setting the field as
    // your highwater field.
    $this->highwaterField = array(
      'name' => 'last_changed', // Column to be used as highwater mark
      'alias' => 'w',           // Table alias containing that column
    );
    // Note that it is important to process rows in the order of the highwater mark
    $query->orderBy('last_changed');

    $this->source = new MigrateSourceSQL($query, $source_fields, $count_query);
    $this->destination = new MigrateDestinationNode('migrate_example_wine');

    // Mapped fields
    $this->addFieldMapping('title', 'name')
         ->description(t('Mapping wine name in source to node title'));
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUser')
         ->defaultValue(1);
    // TIP: By default, term relationship are assumed to be passed by name.
    // In this case, the source values are IDs, so we specify the relevant
    // migration (so the tid can be looked up in the map), and tell the term
    // field handler that it is receiving tids instead of names
    $this->addFieldMapping('migrate_example_wine_varieties', 'variety')
         ->sourceMigration('WineVariety')
         ->arguments(array('source_type' => 'tid'));
    $this->addFieldMapping('migrate_example_wine_regions', 'region')
         ->sourceMigration('WineRegion')
         ->arguments(array('source_type' => 'tid'));
    $this->addFieldMapping('migrate_example_wine_best_with', 'best_with')
         ->separator(',')
         ->sourceMigration('WineBestWith')
         ->arguments(array('source_type' => 'tid'));
    $this->addFieldMapping('field_migrate_example_wine_ratin', 'rating');
    $this->addFieldMapping('field_migrate_example_top_vintag', 'best_vintages');
    $arguments = MigrateTextFieldHandler::arguments(array('source_field' => 'excerpt'));
    $this->addFieldMapping('body', 'body')
         ->arguments($arguments);
    $this->addFieldMapping(NULL, 'excerpt');
    $arguments = MigrateFileFieldHandler::arguments(drupal_get_path('module', 'migrate_example'), 'file_copy', FILE_EXISTS_RENAME);
    $this->addFieldMapping('field_migrate_example_image', 'image')
         ->arguments($arguments);
    $this->addFieldMapping('sticky')
         ->defaultValue(0);
    // These are already UNIX timestamps, so just pass through
    $this->addFieldMapping('created', 'posted');
    $this->addFieldMapping('changed', 'last_changed');

    // No unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('is_new')
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

  // TIP: Implement a prepareRow() method to manipulate the source row between
  // retrieval from the database and the automatic applicaton of mappings
  public function prepareRow($current_row) {
    // We can only handle a single multi-value source field using GROUP_CONCAT
    // as we did above - insert others with a query against the related table
    // with multiple values here, so the values can run through the mapping process
    $source_id = $current_row->wineid;
    $result = db_select('migrate_example_wine_vintages', 'v')
              ->fields('v', array('vintage'))
              ->condition('wineid', $source_id)
              ->execute();
    foreach ($result as $row) {
      $current_row->best_vintages[] = $row->vintage;
    }
    // We could also have used this function to decide to skip a row, in cases
    // where that couldn't easy be done through the original query. Simply
    // return FALSE in such cases.
    return TRUE;
  }
}

class WineCommentMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = 'Comments about wines';
    $this->dependencies = array('WineUser', 'WineWine');
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
             ->fields('wc', array('commentid', 'comment_parent', 'name', 'mail',
              'accountid', 'body', 'wineid', 'subject', 'commenthost', 'userpage',
              'posted', 'lastchanged'))
             ->orderBy('comment_parent');
    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationComment('comment_node_migrate_example_wine');

    // Mapped fields
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('subject', 'subject');
    $this->addFieldMapping('mail', 'mail');
    $this->addFieldMapping('status')
         ->defaultValue(COMMENT_PUBLISHED);
    $this->addFieldMapping('nid', 'wineid')
         ->sourceMigration('WineWine');
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUser')
         ->defaultValue(0);
    $this->addFieldMapping('pid', 'comment_parent')
         ->sourceMigration('WineComment')
         ->description('Parent comment');
    $this->addFieldMapping('comment_body', 'body');
    $this->addFieldMapping('hostname', 'commenthost');
    $this->addFieldMapping('created', 'posted');
    $this->addFieldMapping('changed', 'lastchanged');
    $this->addFieldMapping('homepage', 'userpage');

    // No unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('thread')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
  }
}

class WineFinishMigration extends MigrationBase {
  public function __construct() {
    parent::__construct();
    $this->description = t('If auto_nodetitle is present and was previously enabled,
      re-enable it');
    $this->dependencies = array('WineComment');
  }
  public function isComplete() {
    if (module_exists('auto_nodetitle')) {
      return TRUE;
    }
    else {
      return FALSE;
    }
  }
  public function import() {
    if (!module_exists('auto_nodetitle')) {
      if (WinePrepMigration::$wasEnabled) {
        module_enable(array('auto_nodetitle'));
        $this->showMessage(t('Re-enabled auto_nodetitle module'), 'success');
      }
      else {
        $this->showMessage(t('auto_nodetitle was not originally enabled'), 'success');
      }
    }
    else {
      $this->showMessage(t('Auto_nodetitle module already enabled'), 'success');
    }
    return Migration::RESULT_COMPLETED;
  }
}

/**
 * TIP: This demonstrates a migration designed not to import new content, but
 * to update existing content (in this case, revised wine ratings)
 */
class WineUpdatesMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Update wine ratings');
    $this->dependencies = array('WineWine');
    $this->softDependencies = array('WineFinish');

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

    $query = db_select('migrate_example_wine_updates', 'w')
             ->fields('w', array('wineid', 'rating'));

    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationNode('migrate_example_wine');

    // Indicate we're updating existing data. The default, Migration::SOURCE, would
    // cause existing nodes to be completely replaced by the source data. In this
    // case, the existing node will be loaded and only the rating altered.
    $this->systemOfRecord = Migration::DESTINATION;

    // Mapped fields
    // The destination handler needs the nid to change - since the incoming data
    // has a source id, not a nid, we need to apply the original wine migration
    // mapping to populate the nid.
    $this->addFieldMapping('nid', 'wineid')
         ->sourceMigration('WineWine');
    $this->addFieldMapping('field_migrate_example_wine_ratin', 'rating');

    // No unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('uid');
    $this->addFieldMapping('migrate_example_wine_varieties');
    $this->addFieldMapping('migrate_example_wine_regions');
    $this->addFieldMapping('migrate_example_wine_best_with');
    $this->addFieldMapping('body');
    $this->addFieldMapping('field_migrate_example_image');
    $this->addFieldMapping('sticky');
    $this->addFieldMapping('created');
    $this->addFieldMapping('changed');
    $this->addFieldMapping('is_new')
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

class WineCommentUpdatesMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = 'Update wine comments';
    $this->dependencies = array('WineComment');
    $this->softDependencies = array('WineUpdates');
    $this->map = new MigrateSQLMap($this->machineName,
      array('commentid' => array(
              'type' => 'int',
              'unsigned' => TRUE,
              'not null' => TRUE,
             )
           ),
        MigrateDestinationComment::getKeySchema()
      );
    $query = db_select('migrate_example_wine_comment_updates', 'wc')
             ->fields('wc', array('commentid', 'subject'));
    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationComment('comment_node_migrate_example_wine');
    $this->systemOfRecord = Migration::DESTINATION;

    // Mapped fields
    $this->addFieldMapping('cid', 'commentid')
         ->sourceMigration('WineComment');
    $this->addFieldMapping('subject', 'subject');

    // No unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('name');
    $this->addFieldMapping('mail');
    $this->addFieldMapping('status');
    $this->addFieldMapping('nid');
    $this->addFieldMapping('uid');
    $this->addFieldMapping('pid');
    $this->addFieldMapping('comment_body');
    $this->addFieldMapping('hostname');
    $this->addFieldMapping('created');
    $this->addFieldMapping('changed');
    $this->addFieldMapping('homepage');
    $this->addFieldMapping('thread')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
  }
}

class WineVarietyUpdatesMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Migrate varieties from the source database to taxonomy terms');
    $this->dependencies = array('WineVariety');
    $this->softDependencies = array('WineUpdates');
    $this->map = new MigrateSQLMap($this->machineName,
        array(
          'categoryid' => array('type' => 'int',
                                'unsigned' => TRUE,
                                'not null' => TRUE,
                               )
        ),
        MigrateDestinationTerm::getKeySchema()
      );

    $query = db_select('migrate_example_wine_variety_updates', 'wc')
             ->fields('wc', array('categoryid', 'details'));
    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationTerm('migrate_example_wine_varieties');
    $this->systemOfRecord = Migration::DESTINATION;

    // Mapped fields
    $this->addFieldMapping('tid', 'categoryid')
         ->sourceMigration('WineVariety');
    $this->addFieldMapping('description', 'details');

    // Unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('name');
    $this->addFieldMapping('parent');
    $this->addFieldMapping('weight');
    $this->addFieldMapping('format');
    $this->addFieldMapping('parent_name')
         ->issueGroup(t('DNM'));
  }
}


class WineUserUpdatesMigration extends AdvancedExampleMigration {
  public function __construct() {
    parent::__construct();
    $this->description = t('Account updates');
    $this->dependencies = array('WineUser');
    $this->softDependencies = array('WineUpdates');
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
    $query = db_select('migrate_example_wine_account_updates', 'wa')
             ->fields('wa', array('accountid', 'sex'));
    $this->source = new MigrateSourceSQL($query);
    $this->destination = new MigrateDestinationUser();
    $this->systemOfRecord = Migration::DESTINATION;

    // Mapped fields
    $this->addFieldMapping('uid', 'accountid')
         ->sourceMigration('WineUser');
    $this->addFieldMapping('field_migrate_example_gender', 'sex')
         ->description(t('Map from M/F to 0/1 in prepare method'));

    // Unmapped source fields

    // Unmapped destination fields
    $this->addFieldMapping('name');
    $this->addFieldMapping('status');
    $this->addFieldMapping('created');
    $this->addFieldMapping('access');
    $this->addFieldMapping('login');
    $this->addFieldMapping('mail');
    $this->addFieldMapping('pass');
    $this->addFieldMapping('roles');
    $this->addFieldMapping('signature');
    $this->addFieldMapping('signature_format');
    $this->addFieldMapping('init');
    $this->addFieldMapping('theme')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('timezone')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('picture')
         ->issueGroup(t('DNM'));
  }

  public function prepare(stdClass $account, stdClass $row) {
    // Gender data comes in as M/F, needs to be saved as Male=0/Female=1
    // TIP: Note that the Migration prepare method is called after all other
    // prepare handlers. Most notably, the field handlers have had their way
    // and created field arrays, so we have to save in the same format.
    switch ($row->sex) {
      case 'm':
      case 'M':
        $account->field_migrate_example_gender[LANGUAGE_NONE][0]['value'] = 0;
        break;
      case 'f':
      case 'F':
        $account->field_migrate_example_gender[LANGUAGE_NONE][0]['value'] = 1;
        break;
      default:
        unset($account->field_migrate_example_gender);
        break;
    }
  }
}
