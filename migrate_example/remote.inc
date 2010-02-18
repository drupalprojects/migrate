<?php

class RemoteNodeMigration extends Migration {
  public function __construct() {
    // Always call the parent constructor first - it performs some essential
    // initialization.
    parent::__construct();

    // Human-friendly description of your migration process
    $this->description = 'Example remote fetch node migration';

    // These variables are passed in constructor below. The define how to connect
    // and query the remote database server.
    $countQuery = 'SELECT COUNT(*) as count FROM gw.uc_countries';
    $query = 'SELECT * FROM gw.uc_countries';
    $fields = array('country_id' => 'cid', 'country_name' => 'country_name');
    $query_method = 'mysql_query';
    $fetch_method = 'mysql_fetch_object';
    $remote = variable_get('sql_remote_credentials', array('username' => 'root', 'password' => '', 'servername' => '127.0.0.1'));
    $connection = mysql_connect($remote['servername'], $remote['username'], $remote['password'], FALSE);

    // Use the query as the basis of our MigrateSource. Note that we need to
    // provide a schema definition for the primary key of this source, so the
    // Migration class can track mappings from source data to destination data.
    $this->source = new MigrateSourceSQLRemote($query,
                                  array(
                                    'country_id' => array(
                                      'type' => 'int',
                                      'not null' => TRUE,
                                      'description' => 'Country ID.',
                                    ),
                                  ),
                                  $query_method, $fetch_method, $connection, $countQuery, $fields);

    // Set up our destination - nodes of type migrate_example_beer
    $this->destination = new MigrateDestinationNode('migrate_example_beer');

    // Assign mappings to destination fields from source fields. To discover
    // the names used in these calls, use the drush commands
    // drush migrate-fields-destination beernode
    // drush migrate-fields-source beernode
    $this->addFieldMapping('title', 'country_name');
  }
}
