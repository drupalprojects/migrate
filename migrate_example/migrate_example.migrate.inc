<?php

/**
 * @file
 * Because the name of this file is the module name plus '.migrate.inc', when
 * hook_migrate_api is invoked by the Migrate module this file is automatically
 * loaded - thus, you don't need to implement your hook in the .module file.
 */

/*
 * You must implement hook_migrate_api(), setting the API level to 2, if you are
 * implementing any migration classes. As of Migrate 2.5, you should also
 * register your migration and handler classes explicitly here - the former
 * method of letting them get automatically registered on a cache clear will
 * break in certain environments (see http://drupal.org/node/1778952).
 */
function migrate_example_migrate_api() {
  $api = array(
    'api' => 2,
    // Migrations can be organized into groups. The key used here will be the
    // machine name of the group, which can be used in Drush:
    //  drush migrate-import --group=wine
    // The title is a required argument which is displayed for the group in the
    // UI. You may also have additional arguments for any other data which is
    // common to all migrations in the group.
    'groups' => array(
      'beer' => array(
        'title' => t('Beer Migrations'),
      ),
      'wine' => array(
        'title' => t('Wine Migrations'),
      ),
    ),
    // Here we register the individual migrations. The keys (BeerTerm, BeerUser,
    // etc.) are the machine names of the migrations, and the class_name
    // argument is required. The group_name is optional (defaulting to 'default')
    // but specifying it is a best practice.
    'migrations' => array(
      'BeerTerm' => array(
        'class_name' => 'BeerTermMigration',
        'group_name' => 'beer',
      ),
      'BeerUser' => array(
        'class_name' => 'BeerUserMigration',
        'group_name' => 'beer',
      ),
      'BeerNode' => array(
        'class_name' => 'BeerNodeMigration',
        'group_name' => 'beer',
      ),
      'BeerComment' => array(
        'class_name' => 'BeerCommentMigration',
        'group_name' => 'beer',
      ),
      'WinePrep' => array(
        'class_name' => 'WinePrepMigration',
        'group_name' => 'wine',
      ),
      'WineVariety' => array(
        'class_name' => 'WineVarietyMigration',
        'group_name' => 'wine',
      ),
      'WineRegion' => array(
        'class_name' => 'WineRegionMigration',
        'group_name' => 'wine',
      ),
      'WineBestWith' => array(
        'class_name' => 'WineBestWithMigration',
        'group_name' => 'wine',
      ),
      'WineFileCopy' => array(
        'class_name' => 'WineFileCopyMigration',
        'group_name' => 'wine',
      ),
      'WineFileBlob' => array(
        'class_name' => 'WineFileBlobMigration',
        'group_name' => 'wine',
      ),
      'WineRole' => array(
        'class_name' => 'WineRoleMigration',
        'group_name' => 'wine',
      ),
      'WineUser' => array(
        'class_name' => 'WineUserMigration',
        'group_name' => 'wine',
      ),
      'WineProducer' => array(
        'class_name' => 'WineProducerMigration',
        'group_name' => 'wine',
      ),
      'WineProducerXML' => array(
        'class_name' => 'WineProducerXMLMigration',
        'group_name' => 'wine',
      ),
      'WineProducerMultiXML' => array(
        'class_name' => 'WineProducerMultiXMLMigration',
        'group_name' => 'wine',
      ),
      'WineProducerXMLPull' => array(
        'class_name' => 'WineProducerXMLPullMigration',
        'group_name' => 'wine',
      ),
      'WineWine' => array(
        'class_name' => 'WineWineMigration',
        'group_name' => 'wine',
      ),
      'WineComment' => array(
        'class_name' => 'WineCommentMigration',
        'group_name' => 'wine',
      ),
      'WineTable' => array(
        'class_name' => 'WineTableMigration',
        'group_name' => 'wine',
      ),
      'WineFinish' => array(
        'class_name' => 'WineFinishMigration',
        'group_name' => 'wine',
      ),
      'WineUpdates' => array(
        'class_name' => 'WineUpdatesMigration',
        'group_name' => 'wine',
      ),
      'WineCommentUpdates' => array(
        'class_name' => 'WineCommentUpdatesMigration',
        'group_name' => 'wine',
      ),
      'WineVarietyUpdates' => array(
        'class_name' => 'WineVarietyUpdatesMigration',
        'group_name' => 'wine',
      ),
      'WineUserUpdates' => array(
        'class_name' => 'WineUserUpdatesMigration',
        'group_name' => 'wine',
      ),
    ),
  );
  return $api;
}
