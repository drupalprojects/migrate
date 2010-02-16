<?php
// $Id$

/**
 * @file
 * Defines the base class for destination handlers.
 */

/**
 * Abstract base class for destination handlers. Handler objects are expected
 * to implement prepare and/or complete methods.
 */
abstract class MigrateHandler {
  /**
   * List of other handler classes which should be invoked before the current one.
   *
   * @var array
   */
  public $dependencies = array();

  /**
   * Signatures of methods to implement - cannot be declared here, because
   * a derived class only needs to define one of them.
   */
  // abstract public function prepare(Migration $migration, stdClass $object, stdClass $row);
  // abstract public function complete(Migration $migration, stdClass $object, stdClass $row);

  /**
   * Make sure one of the required methods is implemented
   */
  public function __construct() {
    if (!method_exists($this, 'prepare') && !method_exists($this, 'complete')) {
      throw new Exception(t('!class_name must implement a prepare or complete method',
        array('!class_name' => get_class($this))));
    }
  }
}
