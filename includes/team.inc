<?php
// $Id$

/**
 * @file
 * Support for migration participants.
 */

class MigrateTeamMember {
  public $name;
  public $emailAddress;

  public function __construct($name, $email_address) {
    $this->name = $name;
    $this->emailAddress = $email_address;
  }

  public function contact($subject, $text) {
    // TODO
  }
}
