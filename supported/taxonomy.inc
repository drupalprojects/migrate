<?php
// $Id$

// Note: this file is included both for taxonomy.module and for
// the wrapper taxonomy.module of category.module. This file
// handles both cases for assigning terms to nodes.

/**
 * Implementation of hook_migrate_fields().
 */
function taxonomy_migrate_destination_fields_node($type) {
  $fields = array();
  foreach ((array)taxonomy_get_vocabularies($type) as $vocab) {
    $fields['migrate_taxonomy_'. $vocab->vid] = t('Taxonomy: %name', array('%name' => check_plain($vocab->name)));
  }
  return $fields;
}

/**
 * Implementation of hook_migrate_prepare().
 */
function taxonomy_migrate_destination_prepare_node(&$node, $tblinfo, $row) {
  static $vocabs;
  if (!isset($vocabs)) {
    $vocabs = taxonomy_get_vocabularies($node->type);
  }
  if (!$vocabs || count($vocabs) < 1) return;
  
  $multiple_separator = variable_get('migrate_multiple_separator', '|');

  $taxonomy = array(); // Here we will store the final taxonomy for this node.
  $errors = array();   // Here we will store the errors for this node.

  // It is possible there appeared some terms magically already.
  if (isset($node->taxonomy)) {
    $taxonomy = is_array($node->taxonomy) ? $node->taxonomy : array();
  }

  foreach ((array)$vocabs as $vocab) {
    $field = 'migrate_taxonomy_'. $vocab->vid;
    if (isset($field) && isset($node->$field)) {
      $value = trim($node->$field);
    } else {
      $value = '';
    }
    unset($node->$field);

    $vid = $vocab->vid;

    // Depending on the type of vocabulary, we need to handle this specially.
    if ($vocab->tags) {
      // 1. Free tagging vocabularies:
      //    $node->taxonomy['tags'] = array($vid1 => $text_value, $vid2 => $text_value, ...);
      //    note: we don't have to split the $text_value as taxonomy_node_save()
      //    will do that for us. So in this case, to specify multiple terms, you
      //    need to set it to "term 1, term 2, term 3" (separator = ',').
      $taxonomy['tags'] = isset($taxonomy['tags']) ? $taxonomy['tags'] : array();
      $taxonomy['tags'][$vid] = (!empty($global_value) && !empty($value)) ? ',' : '';
      $taxonomy['tags'][$vid] .= str_replace($multiple_separator, ',', $value);

      // Error if the vocabulary was required, but there are no terms.
      if ($vocab->required && empty($taxonomy['tags'][$vid])) {
        $errors[] = migrate_message(t('You need to assign at least one term of the vocabulary %name.', 
          array('%name' => theme('placeholder', $vocab->name))));
      }
    }
    else {
      // 2. Other vocabularies:
      //    $node->taxonomy = array($tid1, $tid2, ...)
      //    or
      //    $node->taxonomy = array($vid1 => array($tid1, $tid2, ...), $vid2 => array(...), ...)
      //    We'll use the second form.
      $taxonomy[$vid] = isset($taxonomy[$vid]) ? $taxonomy[$vid] : array();
      if (isset($value) && !empty($value)) {
        // If the vocabulary allows multiple terms, explode the $value.
        if ($vocab->multiple) {
          $terms = array_map('trim', explode($multiple_separator, $value));
        }
        else {
          $terms = array($value);
        }

        // Now handle each term.
        foreach ($terms as $text) {
          if (!empty($text)) {
            $tid = _migrate_taxonomy_get_term($vocab, $text, $options['handler']);
            if ($tid >= 0) {
              // A $tid == 0 means that the term was not found, but will be created.
              // Because we check whether terms are assigned later on for required
              // vocabularies, we need to add it to the array.
              $taxonomy[$vid][] = $tid;
            }
            else if ($tid < 0 && $options['handler'] == 'no-import') {
              $errors[] = migrate_message(t('The term %term does not exist in the %name vocabulary.', 
                array('%term' => theme('placeholder', $value), '%name' => theme('placeholder', $vocab->name))));
            }
          }
        }
      }


      // Error if the vocabulary was required, but there are no terms.
      if ($vocab->required && count($taxonomy[$vid]) == 0) {
        $errors[] = migrate_message(t('You need to assign at least one term of the %name vocabulary.', 
          array('%name' => theme('placeholder', $vocab->name))));
      }

      // Make sure there are no duplicated entries and no '0' entries.
      $taxonomy[$vid] = array_filter(array_unique($taxonomy[$vid]));

      // If single select, the $taxonomy[$vid] should be an integer, not an array.
      if (!$vocab->multiple) {
        if (count($taxonomy[$vid]) == 1) {
          $taxonomy[$vid] = $taxonomy[$vid][0];
        }
        else {
          unset($taxonomy[$vid]);
        }
      }
    }
  }

  if (module_exists('category')) {
    $node->category = $taxonomy;
  }
  else {
    $node->taxonomy = $taxonomy;
  }

  return $errors;
}

/**
 * Return a tid for a term (text).
 *
 * Returns 0 if the term will be added.
 * Returns $tid > 0 if the term exists in this vocabulary.
 * Returns -1 if the term does not exist and will not be added.
 */
function _migrate_taxonomy_get_term($vocab, $text, $handler) {
  static $missing_terms = array();

  $vid = $vocab->vid;

  if (!isset($missing_terms[$vid])) {
    $missing_terms[$vid] = array();
  }

  // Bail out for empty text.
  if (empty($text)) {
    return -1;
  }

  // If we have found this $text already, return it.
  if (isset($missing_terms[$vid][$text])) {
    return $missing_terms[$vid][$text];
  }

  // Try to find a term with a matching name.
  $possibilities = taxonomy_get_term_by_name($text);
  foreach ($possibilities as $possibility) {
    if ($possibility->vid == $vid) {
      $missing_terms[$vid][$text] = $possibility->tid;
      return $possibility->tid;
    }
  }

  // Try to find a term with a matching tid.
  if (is_numeric($text) && ($term = taxonomy_get_term($text))) {
    $missing_terms[$vid][$text] = $term->tid;
    return $term->tid;
  }

  // If we arrive here, the term does not exist.
  switch ($handler) {
    case 'add':
      $edit = array('vid' => $vid, 'name' => $text);
      $status = taxonomy_save_term($edit);
      $tid = $edit['tid'];
      drupal_set_message(t('Added %term term to the %name vocabulary.', array('%term' => theme('placeholder', $text), '%name' => theme('placeholder', $vocab->name))));
      break;

    case 'warn':
      drupal_set_message(t('There is no %term term inside the %name vocabulary.', array('%term' => theme('placeholder', $text), '%name' => theme('placeholder', $vocab->name))));
      //Fall-through
    default: // which includes 'ignore' and 'no-import'
      $tid = -1;
      break;
  }

  $missing_terms[$vid][$text] = $tid;
  return $tid;
}
