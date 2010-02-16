<?php
// $Id$

/**
 * @file
 * Custom exception class for the migrate module.
 */

class MigrateException extends Exception {
  public $level;

  public function __construct($message, $level) {
    $this->level = $level;
    parent::__construct($message, 0, NULL);
  }
}
