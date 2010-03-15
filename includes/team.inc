<?php
// $Id$

/**
 * @file
 * Support for migration participants.
 */

class MigrateTeamMember {
  public $name;
  public $emailAddress;
  public $group;

  public function __construct($name, $email_address, $group) {
    $this->name = $name;
    $this->emailAddress = $email_address;
    $this->group = $group;
  }

  public function contact($subject, $text) {
    // TODO
  }
}
