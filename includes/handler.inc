<?php
// $Id$

/**
 * @file
 * Defines the base class for destination handlers.
 */

/**
 * Abstract base class for destination handlers. Handler objects are expected
 * to implement appropriate methods (e.g., prepare, complete, or fields).
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
  // abstract public function fields();

  /**
   * Base constructor
   */
  public function __construct() {}
}
