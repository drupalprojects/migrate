<?php

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
  $pkcol = $tblinfo->pkcol;
  
  $multiple_separator = variable_get('migrate_multiple_separator', '|');

  $taxonomy = array(); // Here we will store the final taxonomy for this node.
  $errors = array();   // Here we will store the errors for this node.

  // It is possible there appeared some terms magically already.
  if (isset($node->taxonomy)) {
    $taxonomy = is_array($node->taxonomy) ? $node->taxonomy : array();
  }

  $options = $node->migrate_taxonomy;
  unset($node->migrate_taxonomy);

  foreach ((array)$vocabs as $vocab) {
    $field = 'migrate_taxonomy_'. $vocab->vid;
    $value = trim($node->$field);
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
      $global_value = trim($options['taxonomy']['tags'][$vid]);
      $taxonomy['tags'][$vid] = $global_value;
      $taxonomy['tags'][$vid] .= (!empty($global_value) && !empty($value)) ? ',' : '';
      $taxonomy['tags'][$vid] .= str_replace($multiple_separator, ',', $value);

      // Error if the vocabulary was required, but there are no terms.
      if ($vocab->required && empty($taxonomy['tags'][$vid])) {
        $errors[] = array($row->$pkcol => t('You need to assign at least one term of the vocabulary %name.', 
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
      $global_value = $options['taxonomy'][$vid];
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
              $errors[] = array($row->$pkcol => t('The term %term does not exist in the %name vocabulary.', 
                array('%term' => theme('placeholder', $value), '%name' => theme('placeholder', $vocab->name))));
            }
          }
        }
      }
      // For !$multiple vocabularies: if $value is not set, use $global_value
      else if (!$vocab->multiple && isset($global_value)) {
        $taxonomy[$vid] = is_array($global_value) ? $global_value : array($global_value);
      }

      // For $multiple vocabularies: add the $global_value if set.
      if ($vocab->multiple && isset($global_value)) {
        $taxonomy[$vid] = array_merge($taxonomy[$vid], (array)$global_value);
      }

      // Error if the vocabulary was required, but there are no terms.
      if ($vocab->required && count($taxonomy[$vid]) == 0) {
        $errors[] = array($row->$pkcol => t('You need to assign at least one term of the %name vocabulary.', 
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

/**
 * Implementation of hook_migrate_global().
 */
function taxonomy_migrate_global($type, $globals) {
  $form = array();
  $vocabs = taxonomy_get_vocabularies($type);
  if ($vocabs && count($vocabs) > 0) {
    $taxonomy = $globals['migrate_taxonomy']['taxonomy'];
    $taxonomy = isset($taxonomy) ? $taxonomy : array();
    $handler = $globals['migrate_taxonomy']['handler'];
    $handler = isset($handler) ? $handler : 'no-import';

    $form['migrate_taxonomy'] = array(
      '#type' => 'fieldset',
      '#title' => t('Taxonomy options'),
      '#description' => t('Select the terms of each vocabulary that will be assigned to each node.'),
      '#tree' => TRUE,
    );

    if (module_exists('category')) {
      $node = (object)array(
        'type' => $type,
        'category' => _migrate_taxonomy_form2node($taxonomy),
      );
      $subform = array(
        'type' => array(
          '#value' => $type,
        ),
        '#node' => $node,
      );
      category_form_alter($subform, $form_state, $type .'_node_form');
      $form['migrate_taxonomy']['taxonomy'] = $subform['category'];
    }
    else {
      $node = (object)array(
        'type' => $type,
        'taxonomy' => _migrate_taxonomy_form2node($taxonomy),
      );
      $subform = array(
        'type' => array(
          '#value' => $type,
        ),
        '#node' => $node,
      );
      taxonomy_form_alter($subform, $form_state, $type .'_node_form');
      $form['migrate_taxonomy']['taxonomy'] = $subform['taxonomy'];
    }

    $options = array(
      'add' => t('Add non-existing terms to the vocabulary'),
      'ignore' => t('Ignore non-existing terms'),
      'warn' => t('Warn me about non-existing terms before the import'),
      'no-import' => t('Do not import the node if there are non-existing terms'),
    );
    $form['migrate_taxonomy']['handler'] = array(
      '#type' => 'select',
      '#title' => t('How to handle non-existing terms?'),
      '#options' => $options,
      '#default_value' => $handler,
      '#description' => t('Select how to handle non-existing terms in the vocabularies. If unsure, select %no-import.', array('%no-import' => theme('placeholder', t('Do not import the node if there are non-existing terms')))),
    );
  }
  return $form;
}

/**
 * Convert the taxonomy array from the form form to the node form.
 *
 * Form form:
 *   $node->taxonomy[$vid] = array($tid, $tid, ...); // multiple select
 *   $node->taxonomy[$vid] = $tid;                   // single select
 *   $node->taxonomy['tags'][$vid] = $text;          // free tagging
 *
 * Node form:
 *   $node->taxonomy[$tid] = $term_object;           // all selects
 *   $node->taxonomy['tags'][$vid] = $text;          // is handled ok
 */
function _migrate_taxonomy_form2node($taxonomy) {
  $tids = array();
  foreach ($taxonomy as $key => $value) {
    if ($key != 'tags') {
      $value = is_array($value) ? $value : array($value);
      foreach ($value as $tid) {
        $tids[$tid] = taxonomy_get_term($tid);
      }
    }
    else {
      $tids[$key] = $value;
    }
  }
  return $tids;
}

