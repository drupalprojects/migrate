<?php
// $Id$

/**
 * @file
 * Set up for the wine (advanced) example.
 */
function migrate_example_wine_schema() {
  $schema['migrate_example_wine_account'] = migrate_example_wine_schema_account();
  $schema['migrate_example_wine_categories'] = migrate_example_wine_schema_categories();
  $schema['migrate_example_wine'] = migrate_example_wine_schema_wine();
  $schema['migrate_example_wine_producer'] = migrate_example_wine_schema_producer();
  $schema['migrate_example_wine_category_wine'] = migrate_example_wine_schema_category_wine();
  $schema['migrate_example_wine_category_producer'] = migrate_example_wine_schema_category_producer();
  $schema['migrate_example_wine_comment'] = migrate_example_wine_schema_comment();

  return $schema;
}

function migrate_example_wine_install() {
  migrate_example_wine_content_types();
  migrate_example_wine_categories();
  migrate_example_wine_image();

  // Populate our tables.
  migrate_example_wine_data_account();
  migrate_example_wine_data_categories();
  migrate_example_wine_data_wine();
  migrate_example_wine_data_producer();
  migrate_example_wine_data_category_wine();
  migrate_example_wine_data_category_producer();
  migrate_example_wine_data_comment();
}

function migrate_example_wine_uninstall() {
  migrate_example_wine_content_type_delete();
  if ($vids = taxonomy_vocabulary_load_multiple(array(), array('machine_name' => 'migrate_example_wine_varieties'))) {
    // Grab key of the first returned vocabulary.
    taxonomy_vocabulary_delete(key($vids));
  }
  if ($vids = taxonomy_vocabulary_load_multiple(array(), array('machine_name' => 'migrate_example_wine_regions'))) {
    // Grab key of the first returned vocabulary.
    taxonomy_vocabulary_delete(key($vids));
  }
  if ($vids = taxonomy_vocabulary_load_multiple(array(), array('machine_name' => 'migrate_example_wine_best_with'))) {
    // Grab key of the first returned vocabulary.
    taxonomy_vocabulary_delete(key($vids));
  }
}

function migrate_example_wine_schema_wine() {
  return array(
    'description' => 'Wines of the world',
    'fields' => array(
      'wineid'  => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'Wine ID',
      ),
      'name'  => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
      ),
      'body' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Full description of the wine.',
      ),
      'excerpt' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Abstract for this wine.',
      ),
      'accountid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => FALSE,
        'description' => 'Account ID of the author.',
      ),
      'image' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Image path',
      ),
    ),
    'primary key' => array('wineid'),
  );
}

function migrate_example_wine_schema_producer() {
  return array(
    'description' => 'Wine producers of the world',
    'fields' => array(
      'producerid'  => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'Producer ID',
      ),
      'name'  => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
      ),
      'body' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Full description of the producer.',
      ),
      'excerpt' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Abstract for this producer.',
      ),
      'accountid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => FALSE,
        'description' => 'Account ID of the author.',
      ),
    ),
    'primary key' => array('producerid'),
  );
}

function migrate_example_wine_schema_categories() {
  return array(
    'description' => 'Categories',
    'fields' => array(
      'categoryid' => array(
        'type' => 'int',
        'not null' => TRUE,
        'unsigned' => TRUE,
        'description' => 'Category ID',
      ),
      'type' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
        'description' => 'Type of category: variety, region, best_with',
      ),
      'name'  => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
      ),
      'details' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
      ),
      'category_parent' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => FALSE,
        'description' => 'Parent category, if any',
      ),
    ),
    'primary key' => array('categoryid'),
  );
}

function migrate_example_wine_schema_category_wine() {
  return array(
    'description' => 'Wine category assignments',
    'fields' => array(
      'wineid'  => array(
        'type' => 'int',
        'not null' => TRUE,
        'description' => 'Wine ID',
      ),
      'categoryid'  => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'Category ID',
      ),
    ),
    'primary key' => array('categoryid', 'wineid'),
  );
}

function migrate_example_wine_schema_category_producer() {
  return array(
    'description' => 'Producer category assignments',
    'fields' => array(
      'producerid'  => array(
        'type' => 'int',
        'not null' => TRUE,
        'description' => 'Producer ID',
      ),
      'categoryid'  => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'Category ID',
      ),
    ),
    'primary key' => array('categoryid', 'producerid'),
  );
}

function migrate_example_wine_schema_comment() {
  return array(
    'description' => 'Wine comments',
    'fields' => array(
      'commentid'  => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'Comment ID',
      ),
      'wineid'  => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'description' => 'Wine ID that is being commented upon',
      ),
      'comment_parent' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => FALSE,
        'description' => 'Parent comment ID in case of comment replies.',
      ),
      'subject' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Comment subject',
      ),
      'body' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Comment body',
      ),
      'name' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Comment name (if anon)',
      ),
      'mail' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Comment email (if anon)',
      ),
      'accountid' => array(
        'type' => 'int',
        'length' => 11,
        'not null' => FALSE,
        'description' => 'Account ID (if any).',
      ),
    ),
    'primary key' => array('commentid'),
  );
}

function migrate_example_wine_schema_account() {
  return array(
    'description' => 'Wine accounts.',
    'fields' => array(
      'accountid'  => array(
        'type' => 'serial',
        'not null' => TRUE,
        'description' => 'Account ID',
      ),
      'status'  => array(
        'type' => 'int',
        'length' => 11,
        'not null' => TRUE,
        'description' => 'Blocked_Allowed',
      ),
      'posted' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
        'description' => 'Registration date',
      ),
      'name' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Account name (for login)',
      ),
      'nickname' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Account name (for display)',
      ),
      'password' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Account password (raw)',
      ),
      'mail' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => FALSE,
        'description' => 'Account email',
      ),
    ),
    'primary key' => array('accountid'),
  );
}

function migrate_example_wine_content_types() {
  // This code based on from standard.profile.
  // Insert default user-defined node types into the database.
  $types = array();
  $types[] = array(
    'type' => 'migrate_example_wine',
    'name' => st('Wine'),
    'base' => 'node_content',
    'description' => st("Wine is what we drink."),
    'custom' => 1,
    'modified' => 1,
    'locked' => 1,
  );
  $types[] = array(
    'type' => 'migrate_example_producer',
    'name' => st('Wine Producer'),
    'base' => 'node_content',
    'description' => st("Wineries, vineyards, and large producers"),
    'custom' => 1,
    'modified' => 1,
    'locked' => 1,
  );

  foreach ($types as $type) {
    $type = node_type_set_defaults($type);
    node_type_save($type);
    node_add_body_field($type);
  }
}

function migrate_example_wine_categories() {
  // Create vocabularies for variety, region, and "best with"
  $description = st('Wine varieties');
  $help = st('Select the variety of this wine');
  $vocabulary = (object) array(
    'name' => 'Migrate Example Wine Varieties',
    'description' => $description,
    'machine_name' => 'migrate_example_wine_varieties',
    'help' => $help,
  );
  taxonomy_vocabulary_save($vocabulary);
  if (!field_info_field('migrate_example_wine_varieties')) {
    $field = array(
      'field_name' => $vocabulary->machine_name,
      'type' => 'taxonomy_term_reference',
      'cardinality' => 1,
      'settings' => array(
        'allowed_values' => array(
          array(
            'vid' => $vocabulary->vid,
            'parent' => 0,
          ),
        ),
      ),
    );
    field_create_field($field);
  }
  if (!field_info_instance('node', 'migrate_example_wine_varieties', 'migrate_example_wine')) {
    $instance = array(
      'field_name' => $vocabulary->machine_name,
      'entity_type' => 'node',
      'label' => $vocabulary->name,
      'bundle' => 'migrate_example_wine',
      'description' => $vocabulary->help,
      'widget' => array(
        'type' => 'taxonomy_autocomplete',
      ),
    );
    field_create_instance($instance);
  }

  $description = st('Wine regions');
  $help = st('Select the region this wine comes from');
  $vocabulary = (object) array(
    'name' => 'Migrate Example Wine Regions',
    'description' => $description,
    'machine_name' => 'migrate_example_wine_regions',
    'help' => $help,
  );
  taxonomy_vocabulary_save($vocabulary);
  if (!field_info_field('migrate_example_wine_regions')) {
    $field = array(
      'field_name' => $vocabulary->machine_name,
      'type' => 'taxonomy_term_reference',
      'cardinality' => 1,
      'settings' => array(
        'allowed_values' => array(
          array(
            'vid' => $vocabulary->vid,
            'parent' => 0,
          ),
        ),
      ),
    );
    field_create_field($field);
  }
  if (!field_info_instance('node', 'migrate_example_wine_regions', 'migrate_example_wine')) {
    $instance = array(
      'field_name' => $vocabulary->machine_name,
      'entity_type' => 'node',
      'label' => $vocabulary->name,
      'bundle' => 'migrate_example_wine',
      'description' => $vocabulary->help,
      'widget' => array(
        'type' => 'taxonomy_autocomplete',
      ),
    );
    field_create_instance($instance);
  }
  if (!field_info_instance('node', 'migrate_example_wine_regions', 'migrate_example_producer')) {
    $instance = array(
      'field_name' => $vocabulary->machine_name,
      'entity_type' => 'node',
      'label' => $vocabulary->name,
      'bundle' => 'migrate_example_producer',
      'description' => $vocabulary->help,
      'widget' => array(
        'type' => 'taxonomy_autocomplete',
      ),
    );
    field_create_instance($instance);
  }

  $description = st('Foods the wine goes best with');
  $help = st('Enter any foods this wine may be paired with, separated by commas');
  $vocabulary = (object) array(
    'name' => 'Migrate Example Wine Best With',
    'description' => $description,
    'machine_name' => 'migrate_example_wine_best_with',
    'help' => $help,
  );
  taxonomy_vocabulary_save($vocabulary);
  if (!field_info_field('migrate_example_wine_best_with')) {
    $field = array(
      'field_name' => $vocabulary->machine_name,
      'type' => 'taxonomy_term_reference',
      'cardinality' => FIELD_CARDINALITY_UNLIMITED,
      'settings' => array(
        'allowed_values' => array(
          array(
            'vid' => $vocabulary->vid,
            'parent' => 0,
          ),
        ),
      ),
    );
    field_create_field($field);
  }
  if (!field_info_instance('node', 'migrate_example_wine_best_with', 'migrate_example_wine')) {
    $instance = array(
      'field_name' => $vocabulary->machine_name,
      'entity_type' => 'node',
      'label' => $vocabulary->name,
      'bundle' => 'migrate_example_wine',
      'description' => $vocabulary->help,
      'widget' => array(
        'type' => 'taxonomy_autocomplete',
      ),
    );
    field_create_instance($instance);
  }
}

// Create an image field named "Migrate Example Image", enabled for the 'wine' content type.
function migrate_example_wine_image() {
  if (!field_info_field('field_migrate_example_image')) {
    $field = array(
      'field_name' => 'field_migrate_example_image',
      'type' => 'image',
      'cardinality' => 1,
      'translatable' => TRUE,
      'indexes' => array('fid' => array('fid')),
      'settings' => array(
        'uri_scheme' => 'public',
        'default_image' => FALSE,
      ),
    );
    field_create_field($field);
  }

  if (!field_info_instance('node', 'field_migrate_example_image', 'migrate_example_wine')) {
    $instance = array(
      'field_name' => 'field_migrate_example_image',
      'entity_type' => 'node',
      'label' => 'Image',
      'bundle' => 'migrate_example_wine',
      'description' => 'Upload an image to go with this wine.',
      'settings' => array(
        'file_directory' => 'field/migrate_example/image',
        'file_extensions' => 'png gif jpg jpeg',
        'max_filesize' => '',
        'max_resolution' => '',
        'min_resolution' => '',
        'alt_field' => TRUE,
        'title_field' => '',
      ),

      'widget' => array(
        'type' => 'image_image',
        'settings' => array(
          'progress_indicator' => 'throbber',
          'preview_image_style' => 'thumbnail',
        ),
        'weight' => -1,
      ),

      'display' => array(
        'full' => array(
          'label' => 'hidden',
          'type' => 'image__large',
          'settings' => array(),
          'weight' => -1,
        ),
        'teaser' => array(
          'label' => 'hidden',
          'type' => 'image_link_content__medium',
          'settings' => array(),
          'weight' => -1,
        ),
        'rss' => array(
          'label' => 'hidden',
          'type' => 'image__large',
          'settings' => array(),
          'weight' => -1,
        ),
        'search_index' => array(
          'label' => 'hidden',
          'type' => 'image__large',
          'settings' => array(),
          'weight' => -1,
        ),
        'search_results' => array(
          'label' => 'hidden',
          'type' => 'image__large',
          'settings' => array(),
          'weight' => -1,
        ),
      ),
    );
    field_create_instance($instance);
  }
}

function migrate_example_wine_content_type_delete() {
  $bundle = 'migrate_example_wine';
  $field_names = array('migrate_example_wine_varieties',
    'migrate_example_wine_regions', 'migrate_example_wine_best_with',
    'field_migrate_example_image');
  foreach ($field_names as $field_name) {
    $instance = field_info_instance('node', $field_name, $bundle);
    field_delete_instance($instance);
    field_delete_field($field_name);
  }
  node_type_delete($bundle);
  $bundle = 'migrate_example_producer';
  $field_names = array('migrate_example_wine_regions');
  foreach ($field_names as $field_name) {
    $instance = field_info_instance('node', $field_name, $bundle);
    field_delete_instance($instance);
    field_delete_field($field_name);
  }
  node_type_delete($bundle);
}

function migrate_example_wine_data_wine() {
  $fields = array('wineid', 'name', 'body', 'excerpt', 'accountid', 'image');
  $query = db_insert('migrate_example_wine')
    ->fields($fields);
  $data = array(
    array(1, 'Montes Classic Cabernet Sauvignon', 'Intense ruby-red color', 'Great!', 1, NULL),
  );
  foreach ($data as $row) {
    $query->values(array_combine($fields, $row));
  }
  $query->execute();
}

function migrate_example_wine_data_producer() {
  $fields = array('producerid', 'name', 'body', 'excerpt', 'accountid');
  $query = db_insert('migrate_example_wine_producer')
    ->fields($fields);
  $data = array(
    array(1, 'Montes', 'Fine Chilean winery', 'Great!', 1),
  );
  foreach ($data as $row) {
    $query->values(array_combine($fields, $row));
  }
  $query->execute();
}

function migrate_example_wine_data_account() {
  $fields = array('accountid', 'status', 'posted', 'name', 'nickname', 'password', 'mail');
  $query = db_insert('migrate_example_wine_account')
    ->fields($fields);
  $data = array(
    array(1, 1, '2010-03-30 10:31:05', 'darren', 'Daring Darren', 'dpass', 'darren@example.com'),
    array(3, 0, '2007-03-15 10:31:05', 'emily', 'efindher', 'insecure', 'emily@example.com'),
    array(4, 1, '2004-02-29 10:31:05', 'fonzie', 'Eh!', 'bike', 'thefonz@example.com'),
  );
  foreach ($data as $row) {
    $query->values(array_combine($fields, $row));
  }
  $query->execute();
}

function migrate_example_wine_data_comment() {
  $fields = array('commentid', 'wineid', 'comment_parent', 'subject', 'body', 'name', 'mail', 'accountid');
  $query = db_insert('migrate_example_wine_comment')
    ->fields($fields);
  $data = array(
    array(1, 1, NULL, 'im first', 'Tasty', 'grace', 'grace@example.com', 0),
    array(2, 1, NULL, 'im second', 'Delicious', 'horace', 'horace@example.com', 0),
    array(3, 1, NULL, 'im parent', 'Don\'t care for it', 'irene', 'irene@example.com', 0),
    array(4, 1, 3, 'im child', 'But it\'s so good!', 'emily', NULL, 3),
    array(5, 1, 4, 'im grandchild', 'Right on, Emily!', 'thefonz@example.com', NULL, 4),
  );
  foreach ($data as $row) {
    $query->values(array_combine($fields, $row));
  }
  $query->execute();
}

function migrate_example_wine_data_categories() {
  $fields = array('categoryid', 'type', 'name', 'category_parent', 'details');
  $query = db_insert('migrate_example_wine_categories')
    ->fields($fields);
  $data = array(
    array(1, 'variety', 'White wine', NULL, 'White wines are generally simpler and sweeter than red'),
    array(3, 'variety', 'Red wine', NULL, 'Red wines are generally more complex and "dry" than white'),
    array(8, 'variety', 'Riesling', 1, 'Associated with Germany'),
    array(9, 'variety', 'Chardonnay', 1, 'One of the most popular whites'),
    array(13, 'variety', 'Merlot', 3, 'Very drinkable'),
    array(14, 'variety', 'Syrah', 3, 'A.k.a. shiraz'),
    array(2, 'region', 'Italy', NULL, 'Largest producer of wine'),
    array(11, 'region', 'Tuscany', 2, NULL),
    array(18, 'region', 'Chianti', 11, NULL),
    array(19, 'region', 'Elba', 11, NULL),
    array(4, 'region', 'France', NULL, 'C\'est bon'),
    array(5, 'region', 'Bordeaux', 4, NULL),
    array(6, 'region', 'Barsac', 5, NULL),
    array(7, 'region', 'Pomerol', 5, NULL),
    array(10, 'best_with', 'Beef', NULL, NULL),
    array(12, 'best_with', 'Pork', NULL, NULL),
    array(15, 'best_with', 'Chicken', NULL, NULL),
  );
  foreach ($data as $row) {
    $query->values(array_combine($fields, $row));
  }
  $query->execute();
}

function migrate_example_wine_data_category_wine() {
  $fields = array('wineid', 'categoryid');
  $query = db_insert('migrate_example_wine_category_wine')
    ->fields($fields);
  $data = array(
    array(1, 13),
    array(1, 6),
    array(1, 12),
  );
  foreach ($data as $row) {
    $query->values(array_combine($fields, $row));
  }
  $query->execute();
}

function migrate_example_wine_data_category_producer() {
  $fields = array('producerid', 'categoryid');
  $query = db_insert('migrate_example_wine_category_producer')
    ->fields($fields);
  $data = array(
    array(1, 6),
  );
  foreach ($data as $row) {
    $query->values(array_combine($fields, $row));
  }
  $query->execute();
}
