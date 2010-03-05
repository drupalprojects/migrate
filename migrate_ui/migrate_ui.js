// $Id$
(function ($) {

/**
 * Provide the summary information for the migration detail vertical tabs.
 */
Drupal.behaviors.migrateUISummary = {
  attach: function (context) {
    // The setSummary method required for this behavior is not available
    // on the Blocks administration page, so we need to make sure this
    // behavior is processed only if setSummary is defined.
    if (typeof jQuery.fn.setSummary == 'undefined') {
      return;
    }

    $('fieldset#edit-overview', context).setSummary(function (context) {
      if (!$('#owner', context).children()) {
        return '<span class="error">' + Drupal.t('Missing client owner.') + '</span>';
      }
    });
    $('fieldset#edit-destination', context).setSummary(function (context) {
      $total = $('tr', context).length - 2;
      $unmapped = $('td.migrate-error', context).length / 2;
      $mapped = $total - $unmapped;
      $msg = Drupal.formatPlural($mapped, '1 mapping.', '@count mapped.');
      if ($unmapped) {
        $msg = '<span class="error">' + Drupal.formatPlural($unmapped, '1 unmapped', '@count unmapped') + '</span>' + '. ' + $msg;
      }
      return $msg;
    });
    $('fieldset#edit-source', context).setSummary(function (context) {
      $total = $('tr', context).length - 2;
      $unmapped = $('td.migrate-error', context).length / 2;
      $mapped = $total - $unmapped;
      $msg = Drupal.formatPlural($mapped, '1 mapping.', '@count mapped.');
      if ($unmapped) {
        $msg = '<span class="error">' + Drupal.formatPlural($unmapped, '1 unmapped', '@count unmapped') + '</span>' + '. ' + $msg;
      }
      return $msg;
    });

    $('fieldset.migrate-mapping').each(function ($context) {
      $total = $(this).find('tr').length - 2;
      $(this).setSummary(Drupal.formatPlural($total, '1 mapping.', '@count mappings.'));
    });
  }
};

})(jQuery);
