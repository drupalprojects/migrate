<?php
// $Id$

/**
 * @file
 * Support for migration from XML sources.
 */

/**
 * Implementation of MigrateSource, to handle imports from XML feeds. This
 * supports a pattern where there is a URL to XML data listing IDs of content
 * to import, and the IDs are then used to construct URLs for obtaining specific
 * source objects.
 */
abstract class MigrateSourceXML extends MigrateSource {
  /**
   * The URL from which to obtain XML data.
   *
   * @var string
   */
  protected $listUrl;
  public function getListUrl() {
    return $this->listUrl;
  }

  /**
   * List of IDs from the listing URL.
   *
   * @var array
   */
  protected $idList = array();
  public function getIdList() {
    return $this->idList;
  }

  /**
   * List of available source fields.
   *
   * @var array
   */
  protected $fields = array();

  /**
   * Keep the current source ID handy
   * @var string
   */
  protected $id;

  /**
   * Simple initialization.
   */
  public function __construct(Migration $migration, $list_url, $fields = array()) {
    parent::__construct($migration);
    $this->listUrl = $list_url;
    $this->fields = $fields;
    // Suppress errors during parsing, so we can pick them up after
    libxml_use_internal_errors(TRUE);
  }

  /**
   * Return a string representing the source.
   *
   * @return string
   */
  public function __toString() {
    return (string) $this->listUrl;
  }

  /**
   * Returns a list of fields available to be mapped from the source query.
   * Since we can't reliably figure out what "source fields" are in the XML source,
   * it's up to the implementing Migration constructor to fill them in.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    // The fields are defined in the constructor for this plugin.
    return $this->fields;
  }

  /**
   * Implementation of Iterator::rewind() - called before beginning a foreach loop.
   */
  public function rewind() {
    $this->currentRow = NULL;
    $this->numProcessed = 0;
    $idlist = $this->migration->getOption('idlist');
    if ($idlist) {
      $this->idList = explode(',', $idlist);
    }
    else {
      // Run query, do first next()
      migrate_instrument_start('MigrateSourceXML list retrieval');

      // Retrieve the XML listing of object IDs to retrieve
      $xml = simplexml_load_file($this->listUrl);
      if ($xml) {
        // Derived class must implement populateIdlist()
        $this->populateIdlist($xml);
      }
      else {
        $this->migration->showMessage(t('Loading of !listurl failed:',
          array('!listurl' => $this->listUrl)));
        foreach (libxml_get_errors() as $error) {
          $this->migration->showMessage($error);
        }
      }

      migrate_instrument_stop('MigrateSourceXML list retrieval');
    }

    // Load up the first row
    $this->next();
  }

  /**
   * Implementation of Iterator::next() - called at the bottom of the loop implicitly,
   * as well as explicitly from rewind().
   */
  public function next() {
    $this->currentRow = NULL;
    $this->currentKey = NULL;

    // Enforce the itemlimit
    $itemlimit = $this->migration->getOption('itemlimit');

    // get next object (next ID not already in the map, unless needs_update=1)
    migrate_instrument_start('MigrateSourceXML next');
    while ($this->id = array_shift($this->idList)) {
      // Enforce the itemlimit
      if ($itemlimit && $this->numProcessed >= $itemlimit) {
        return;
      }
      // Check the map - if it's already mapped, and not marked for update, skip it
      $map_row = $this->migration->getMap()->getRowBySource(array($this->id));
      if ($map_row && $map_row['needs_update'] == 0) {
        continue;
      }
      // TODO: Also check message table (entries here indicate failed items, we don't
      // want to run through them again)
      $object_url = $this->constructObjectUrl($this->id);
      migrate_instrument_start('load object xml');
      // Allow derived class to override for preprocessing of XML
      $xml = $this->loadXmlUrl($object_url);
      migrate_instrument_stop('load object xml');
      $this->numProcessed++;
      if ($xml) {
        $this->currentKey = array($this->id);
        $this->currentRow = new stdclass;
        $this->currentRow->xml = $xml;
        // Save the ID using the map source key - it will be used for mapping
        $sourceKey = $this->migration->getMap()->getSourceKey();
        $key_name = key($sourceKey);
        $this->currentRow->$key_name = $this->id;
        // Add map info to the row, if present
        if ($map_row) {
          foreach ($map_row as $field => $value) {
            $field = 'migrate_map_' . $field;
            $this->currentRow->$field = $value;
          }
        }
        // OK, got a valid one, break out and return
        break;
      }
      else {
        $message =  t('Loading of !objecturl failed:', array('!objecturl' => $object_url));
        foreach (libxml_get_errors() as $error) {
          $message .= "\n" . $error->message;
        }
        $this->migration->getMap()->saveMessage(
          array($this->id), $message, MigrationBase::MESSAGE_ERROR);
        libxml_clear_errors();
        $this->currentKey = NULL;
      }
    }

    migrate_instrument_stop('MigrateSourceXML next');
  }

  /**
   * Default XML loader - just use Simplexml directly
   */
  protected function loadXmlUrl($object_url) {
    return simplexml_load_file($object_url);
  }
}

/**
 * Adds xpath info to field mappings for XML sources
 */
class MigrateXMLFieldMapping extends MigrateFieldMapping {
  /**
   * The xpath used to retrieve the data for this field from the XML.
   *
   * @var string
   */
  protected $xpath;
  public function getXpath() {
    return $this->xpath;
  }

  /**
   * Add an xpath to this field mapping
   *
   * @param string $xpath
   */
  public function xpath($xpath) {
    $this->xpath = $xpath;
    return $this;
  }
}

/**
 * Migrations using XML sources should extend this class instead of Migration.
 */
abstract class XMLMigration extends Migration {
  /**
   * Override the default addFieldMapping(), so we can create our special
   * field mapping class.
   * TODO: Find a cleaner way to just substitute a different mapping class
   *
   * @param string $destinationField
   *  Name of the destination field.
   * @param string $sourceField
   *  Name of the source field (optional).
   */
  protected function addFieldMapping($destination_field, $source_field = NULL) {
    // Warn of duplicate mappings
    if (!is_null($destination_field) && isset($this->fieldMappings[$destination_field])) {
      $this->showMessage(
        t('!name addFieldMapping: !dest was previously mapped, overridden',
          array('!name' => $this->machineName, '!dest' => $destination_field)),
        'warning');
    }
    $mapping = new MigrateXMLFieldMapping($destination_field, $source_field);
    if (is_null($destination_field)) {
      $this->fieldMappings[] = $mapping;
    }
    else {
      $this->fieldMappings[$destination_field] = $mapping;
    }
    return $mapping;
  }

  /**
   * A normal $data_row has all the input data as top-level fields - in this
   * case, however, the data is embedded within a SimpleXMLElement object in
   * $data_row->xml. Explode that out to the normal form, and pass on to the
   * normal implementation.
   *
   * @param stdClass $data_row
   */
  protected function applyMappings(stdClass $data_row) {
    // We only know what data to pull from the xpaths in the mappings.
    foreach ($this->fieldMappings as $mapping) {
      $source = $mapping->getSourceField();
      if ($source) {
        $xpath = $mapping->getXpath();
        if ($xpath) {
          // Derived class may override applyXpath()
          $data_row->$source = $this->applyXpath($data_row, $xpath);
        }
      }
    }
    return parent::applyMappings($data_row);
  }

  /**
   * Default implementation - straightforward xpath application
   *
   * @param $data_row
   * @param $xpath
   */
  public function applyXpath($data_row, $xpath) {
    $result = $data_row->xml->xpath($xpath);
    if ($result) {
      if (count($result) > 1) {
        $return = array();
        foreach ($result as $record) {
          $return[] = (string)$record;
        }
        return $return;
      }
      else {
        return (string)$result[0];
      }
    }
    else {
      return NULL;
    }
  }
}
