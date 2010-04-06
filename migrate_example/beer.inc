<?php
// $Id$

/**
 * @file
 * A basic migration example.
 */

/**
 * To define a migration process from a set of source data to a particular
 * kind of Drupal object (for example, a specific node type), you define
 * a class derived from Migration. You must define a constructor to initialize
 * your migration object. The name of the class must end in "Migration", and the
 * portion of the name before "Migration" will be the "machine name" of your migration -
 * this is the name you will use in drush commands.
 *
 * There are three essential components to set up in your constructor:
 *  $this->source - An instance of a class derived from MigrateSource, this
 *    will feed data to the migration.
 *  $this->destination - An instance of a class derived from MigrateDestination,
 *    this will receive data that originated from the source and has been mapped
 *    by the Migration class, and create Drupal objects.
 *  Mappings - Use $this->addFieldMapping to tell the Migration class what source
 *    columns correspond to what destination Fields, and additional information
 *    associated with the destination objects.
 */
class BeerNodeMigration extends Migration {
  public function __construct() {
    // Always call the parent constructor first - it performs some essential
    // initialization.
    parent::__construct();

    // Human-friendly description of your migration process. Be as detailed as you
    // like.
    $this->description = 'Beers of the world';

    // These names are currently only used for display on migration detail pages.
    $this->team = array(
      new MigrateTeamMember('Drinker Liz', 'drinker@example.com', t('Owner')),
      new MigrateTeamMember('Brewer Larry', 'brewer@example.com', t('Implementor')),
    );

    // You may optionally declare dependencies for your migration - other migrations
    // which should run first. In this case, terms assigned to our nodes and
    // the authors of the nodes should be migrated before the nodes themselves.
    $this->dependencies = array('BeerTermMigration', 'BeerUserMigration');

    // We are getting data from tables in the Drupal default database - first,
    // set up a query for this data.
    $query = db_select('migrate_example_beer', 'b')
             ->fields('b', array('bid', 'name', 'body', 'excerpt', 'aid', 'countries', 'image'));
    $query->leftJoin('migrate_example_topic_beer', 'tb', 'b.bid = tb.bid');
    // Gives a single comma-separated list of terms
    $query->groupBy('tb.bid');
    $query->addExpression('GROUP_CONCAT(tb.name)', 'terms');

    // Use the query as the basis of our MigrateSource. Note that we need to
    // provide a schema definition for the primary key of this source, so the
    // Migration class can track mappings from source data to destination data.
    $this->source = new MigrateSourceSQL($query,
                                  array(
                                    'bid' => array(
                                      'type' => 'int',
                                      'not null' => TRUE,
                                      'description' => 'Beer ID.',
                                      'alias' => 'b',
                                    )
                                  ));
    // By default, MigrateSourceSQL derives a count query from the main query -
    // but we can override it
    $this->source->countQuery = db_select('migrate_example_beer', 'b');
    $this->source->countQuery->addExpression('COUNT(bid)', 'cnt');

    // Set up our destination - nodes of type migrate_example_beer
    $this->destination = new MigrateDestinationNode('migrate_example_beer');

    // Assign mappings to destination fields from source fields. To discover
    // the names used in these calls, use the drush commands
    // drush migrate-fields-destination beernode
    // drush migrate-fields-source beernode

    // Documenting your mappings makes it easier for the whole team to see
    // exactly what the status is when developing a migration process. The
    // mapping descriptions have an optional group (defaulting to 'Done'), which
    // groups them on the Migration info page.
    $this->addFieldMapping('title', 'name')
         ->description(t('Mapping beer name in source to node title'));

    // You can also document destination fields that are yet to be mapped...
    $this->addFieldMapping('sticky')
         ->description(t('Where is a source field we can map here'))
         ->issueGroup(t('Client questions'))
         ->issueNumber(256)
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_LOW);
    // ... or source fields we don't know what to do with
    $this->addFieldMapping(NULL, 'mysterious_column')
         ->description(t('Talk to client about what this source field means'))
         ->issueGroup(t('Client questions'))
         ->issueNumber(255)
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM);

    // We can use the 'DNM' or 'Do not migrate' group to silence warnings about
    // unmapped source or destination fields
    $this->addFieldMapping('name')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('authorid')
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

    // To maintain node identities between the old and new systems (i.e., have
    // the same unique IDs), map the ID column from the old system to nid and
    // set is_new to TRUE.
    $this->addFieldMapping('nid', 'bid')
         ->description(t('Preserve old beer ID as nid in Drupal'));
    $this->addFieldMapping('is_new')
         ->defaultValue(TRUE);

    // References to related objects (such as the author of the content) are
    // most likely going to be identifiers from the source data, not Drupal
    // identifiers (such as uids). You can use the mapping from the relevant
    // migration to translate from the old ID to the Drupal identifier.
    // Note that we also provide a default value of 1 - if the lookup fails to
    // find a corresponding uid for the aid, the owner will be the administrative
    // account.
    $this->addFieldMapping('uid', 'aid')
         ->defaultValue(1)
         ->sourceMigration('BeerUserMigration');

    // If an incoming field contains multiple values separated by a delimitor,
    // simple specify the delimitor and it will be broken out into an array
    // of values in the destination object.

    // This is a multi-value text field
    $this->addFieldMapping('field_migrate_example_country', 'countries')
         ->separator('|');

    // Taxonomy terms are now implemented as fields.
    $this->addFieldMapping('taxonomy_migrate_example_tags', 'terms')
         ->separator(',');

    // Drupal 7 fields can have attributes such as text formats, or summaries
    // (equivalent to teasers in previous Drupal versions). These can be
    // passed in an arguments array.
    $arguments = MigrateTextFieldHandler::arguments(
      array('source_field' => 'excerpt')); // Sets the summary.
    $this->addFieldMapping('body', 'body')
         ->arguments($arguments);

    // Copy an image file, write DB record to files table, and save in Field storage.
    $arguments = MigrateFileFieldHandler::arguments(drupal_get_path('module', 'migrate_example'), 'file_copy', FILE_EXISTS_RENAME);
    $this->addFieldMapping('field_migrate_example_image', 'image')
         ->arguments($arguments);
  }
}

class BeerUserMigration extends Migration {
  public function __construct() {
    parent::__construct();
    $this->description = 'Beers Drinkers of the world';
    $query = db_select('migrate_example_account', 'mea')
             ->fields('mea', array('aid', 'status', 'posted', 'name', 'nickname', 'password', 'mail', 'sex', 'beers'));
    $this->source = new MigrateSourceSQL($query,
                                  array(
                                    'aid' => array(
                                      'type' => 'int',
                                      'not null' => TRUE,
                                      'description' => 'Account ID.',
                                    )
                                  ));
    $this->destination = new MigrateDestinationUser();
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('status', 'status');
    $this->addFieldMapping('created')
         ->description('See prepare method');
    $this->addFieldMapping('mail', 'mail');
    $this->addFieldMapping('pass', 'password');
    $this->addFieldMapping('roles')
         ->defaultValue(drupal_map_assoc(array(2)));

    // Example of a simple 'value' Field. Used for reference fields, list fields, etc.
    $this->addFieldMapping('field_migrate_example_gender', 'sex');

    // Need to have node_reference module enabled for this
    $this->addFieldMapping('field_migrate_example_favbeers', 'beers')
         ->separator('|');
  }

  // A prepare handler. Alter the entity as needed before save.
  public function prepare(Migration $migration, stdClass $node, stdClass $row) {
    // Source dates are in ISO format and Denver timezone.
    // Convert to unix timestamp.
    date_default_timezone_set('US/Mountain');
    $node->created = strtotime($row->posted);
  }
}


class BeerCommentMigration extends Migration {
  public function __construct() {
    parent::__construct();
    $this->description = 'Comments about beers';
    $this->dependencies = array('BeerUserMigration', 'BeerNodeMigration');
    $query = db_select('migrate_example_comment', 'mec')
             ->fields('mec', array('cid', 'cid_parent', 'name', 'mail', 'aid', 'body', 'bid', 'subject'))
             ->orderBy('cid_parent', 'ASC');
    $this->source = new MigrateSourceSQL($query,
                                  array(
                                    'cid' => array(
                                      'type' => 'int',
                                      'not null' => TRUE,
                                    )
                                  ));
    $this->destination = new MigrateDestinationComment('comment_node_migrate_example_beer');

    //$this->addFieldMapping('name', 'name');
    $this->addFieldMapping('subject', 'subject');
    $this->addFieldMapping('mail', 'mail');
    $this->addFieldMapping('status')
         ->defaultValue(COMMENT_PUBLISHED);

    // We preserved bid => nid ids during BeerNodeMigration so simple mapping suffices.
    $this->addFieldMapping('nid', 'bid');

    $this->addFieldMapping('uid', 'aid')
         ->sourceMigration('BeerUserMigration')
         ->defaultValue(0);
    $this->addFieldMapping('pid', NULL) // 'cid_parent'
         ->sourceMigration('BeerCommentMigration')
         ->arguments(array('cache_size' => 0)) // TODO: No longer have a lookup cache.
         ->description('Parent comment. TODO: not working, so commented cid_parent')
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM)
         ->issueGroup('Implementor issues');

    $this->addFieldMapping('comment_body', 'body');
  }
}

class BeerTermMigration extends Migration {
  public function __construct() {
    parent::__construct();
    $this->description = 'Beer topics';
    // This sort assures that parents are saved before children.
    $query = db_select('migrate_example_topic', 'met')
             ->fields('met', array('name', 'description', 'parent_name'))
             ->orderBy('parent_name', 'ASC');
    $this->source = new MigrateSourceSQL($query,
                                  array(
                                    'name' => array(
                                      'type' => 'varchar',
                                      'length' => 255,
                                      'not null' => TRUE,
                                      'description' => 'Topic ID',
                                    )
                                  ));

    $this->destination = new MigrateDestinationTerm('migrate_example_tags');
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('description', 'description');
    $this->addFieldMapping('parent_name', 'parent_name');
  }
}
