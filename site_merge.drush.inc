<?php

/**
 * @file
 *   Provides Site Merging capabilities.
 */

use Drupal\Core\Config\StorageComparer;
use Drupal\Core\Config\ConfigImporter;
use Drupal\Core\Config\FileStorage;
use Symfony\Component\Yaml\Parser;

/**
 * Implements hook_drush_command().
 */
function site_merge_drush_command() {
  $items = array();
  $items['site-merge-conf'] = array(
    'description' => 'Print configuration path details using print_r().',
    'hidden' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_NONE,
    'options' => array(
      'all' => 'Show all database connections, instead of just one.',
      ),
    'outputformat' => array(
      'default' => 'print-r',
      'pipe-format' => 'var_export',
      'private-fields' => 'password',
    ),
  );

  $items['site-merge'] = array(
    'description' => 'Merge configuration data from two sites or environments.',
    'aliases' => array('sm'),
    'arguments' => array(
      'source' => 'Source Configuration',
      'target' => 'The site config to merge updates from source into.',
    ),
    'options' => array(
      'label' => 'The config label to use. This can be more specific with source-label and target-label.',
      'source-label' => 'The config label to use for the source system',
      'target-label' => 'The config label to use for the destination/target.',
      'ancestor-site' => 'The ancestor site config. Use this carefully. ',
      'ancestor-commit' => 'The location in the git history where the last successful merge took place. If left blank, this will look in the target config for a stored value indicating the last successful merge.',
    ),
    'examples' => array(
      'drush site-merge [sourcelocation] [targetlocation] --ancestor-commit=C1B2DF' => 'Merge configuration changes from the default site with the configuration changes made on the target site.',
    ),
    'topics' => array('docs-sm'),
  );
  $topic_file = __DIR__ . '/docs/sm.md';
  $items['docs-sm'] = array(
    'description' => 'Configuration management on Drupal 8 with Drush.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_NONE,
    'callback' => 'drush_print_file',
    'callback arguments' => array($topic_file),
  );

  return $items;
}

/**
 * Implements hook_drush_help().
 *
 * @param
 *   A string with the help section (prepend with 'drush:')
 *
 * @return
 *   A string with the help text for your command.
 */
function site_merge_drush_help($section) {
  switch ($section) {
    case 'drush:site-merge':
      return dt("Brief help for Drush command site_merge.");
    // The 'title' meta item is used to name a group of
    // commands in `drush help`.  If a title is not defined,
    // the default is "All commands in ___", with the
    // specific name of the commandfile (e.g. config_extra).
    // Command files with less than four commands will
    // be placed in the "Other commands" section, _unless_
    // they define a title.  It is therefore preferable
    // to not define a title unless the file defines a lot
    // of commands.
    case 'meta:site_merge:title':
      return dt("site_merge commands");
    // The 'summary' meta item is displayed in `drush help --filter`,
    // and is used to give a general idea what the commands in this
    // command file do, and what they have in common.
    case 'meta:site_merge:summary':
      return dt("Summary of all commands in this command group.");
  }
}


/**
 * Command callback. Displays the Drupal site's configuration locations.
 * This is necessary in order to 'fake-add' the config values to the two sites
 * specified as parameters.
 */
function drush_site_merge_conf() {
  drush_sql_bootstrap_database_configuration();
  global $config_directories;

  return $config_directories;
}

function drush_site_merge_cache_config_settings(&$alias_record, $config_directories) {
  if (!empty($config_directories)) {
    $alias_record['config_directories'] = $config_directories;
  }

  // If the name is set, then re-cache the record after we fetch the databases
  if (array_key_exists('#name', $alias_record)) {
    $all_site_aliases =& drush_get_context('site-aliases');
    $all_site_aliases['@' . $alias_record['#name']] = $alias_record;
    // Check and see if this record is a copy of 'self'
    if (($alias_record['#name'] != 'self') && array_key_exists('@self', $all_site_aliases) && array_key_exists('#name', $all_site_aliases['@self']) && ($all_site_aliases['@self']['#name'] == $alias_record['#name'])) {
      $all_site_aliases['@self'] = $alias_record;
    }
  }
}

function drush_site_merge_add_config_settings(&$alias_record) {
  $altered_record = FALSE;
  if (isset($alias_record['root'])) {
    // If the alias record does not have a defined 'config_directories' entry,
    // then we'll need to look one up
    if (!isset($alias_record['config_directories']) && !isset($alias_record['site-list'])) {
      $values = drush_invoke_process($alias_record, "site-merge-conf", array(), array('all' => TRUE), array('integrate' => FALSE, 'override-simulated' => TRUE));
      if (is_array($values) && ($values['error_status'] == 0)) {
        $altered_record = TRUE;
        // If there are any special settings in the '@self' record returned by drush_invoke_process,
        // then add those into our altered record as well
        if (array_key_exists('self', $values)) {
          $alias_record = array_merge($values['self'], $alias_record);
        }
        drush_site_merge_cache_config_settings($alias_record, $values['object']);
      }
    }
  }
  return $altered_record;
}


function drush_site_merge_get_config_spec(&$alias_record, $default_to_self = FALSE, $prefix = '') {
  $altered_record = drush_site_merge_add_config_settings($alias_record);

  return array_key_exists('config_directories', $alias_record) ? $alias_record['config_directories'] : NULL;
}



/*
 * Implements COMMAND hook init. The goal here is to allow this command to get
 * settings/config paths for both the source and the target folders.
 *
 * this was copied from sql-sync which does the same thing
 */
function drush_site_merge_init($source, $destination) {
  // Try to get @self defined when --uri was not provided.
  drush_bootstrap_max(DRUSH_BOOTSTRAP_DRUPAL_SITE);

  // Preflight destination in case it defines the alias used by the source
  _drush_sitealias_get_record($destination);

  // After preflight, get source and destination settings
  $source_settings = drush_sitealias_get_record($source);
  $destination_settings = drush_sitealias_get_record($destination);

  // Apply command-specific options.
  drush_sitealias_command_default_options($source_settings, 'source-');
  drush_sitealias_command_default_options($destination_settings, 'target-');
}

function drush_site_merge_validate($source, $destination) {
  $source_settings = drush_sitealias_overlay_options(drush_sitealias_get_record($source), 'source-');
  $destination_settings = drush_sitealias_overlay_options(drush_sitealias_get_record($destination), 'target-');
  $source_configs = drush_site_merge_get_config_spec($source_settings, FALSE, 'source-');
  $target_configs = drush_site_merge_get_config_spec($destination_settings, FALSE, 'target-');
  $source = array();
}

/**
 * Implements drush config-merge command
 *
 * @param $alias
 *   The target site to merge configuration with
 * @param $config_label
 *   Which configuration set (active, etaging, etc.) to operate on
 */
function drush_site_merge($source = '', $destination = '') {
  $source_settings = drush_sitealias_overlay_options(drush_sitealias_get_record($source), 'source-');
  $destination_settings = drush_sitealias_overlay_options(drush_sitealias_get_record($destination), 'target-');
  // get the ancestor site settings (normally will be the same as the source settings)
  $ancestor_settings = drush_sitealias_overlay_options(drush_sitealias_get_record(drush_get_option('ancestor-site', $source)), 'ancestor-');

  $source_configs = drush_site_merge_get_config_spec($source_settings, FALSE, 'source-');
  $target_configs = drush_site_merge_get_config_spec($destination_settings, FALSE, 'target-');
  $ancestor_configs = drush_site_merge_get_config_spec($ancestor_settings, FALSE, 'ancestor-');

  $source_options = drush_get_merged_prefixed_options('source-');
  $target_options = drush_get_merged_prefixed_options('target-');
  $ancestor_options = drush_get_merged_prefixed_options('ancestor-');

  //TODO: add a mark as merged without merging feature, so that sites that don't need the functionality can have it appear to be merged and removed.
  //TODO: add an all sites option

  // This has to do XXX things:
  // 1) determine which site is the parent config
  // 2) determine when they diverged last *
  // 3) determine what site is going to be targeted
  // 4) merge the 3 points together.
  // 5) upon success, overwrite the original config
  // 6) add a reference to the current hash to indicate when this changed *
  // 7) upon failure (conflicts), notify the user and exit.
  //
  // The last merge location will be tricky since technically the NEXT commit will
  // be the correct one. some kind of trace will need to be done through the repo
  // to look for the commit where the site config variable with a commit hash changes next.
  // that will be the one that is the merge point. * maybe that isn't true though
  // the next commit will indicate that the parent at this point was merged in. TBD
  //
  // The following steps need to take place to do this:
  // 1) a temp git repo needs to be created/initialized
  // 2) a copy of code from the last merge point needs to be added to that repo
  // 3) a branch for the target site needs to be created.
  // 4) a copy of the target site needs to be added to the branch.
  // 5) a copy of the current parent needs to be added to the master of the repo.

  // Figure out what our base commit is going to be for this operation.
  $merge_info = array(
    'source-configuration_path' => $source_configs[drush_get_option('source-label', drush_get_option('label', 'vcs'))],
    'target-configuration_path' => $target_configs[drush_get_option('target-label', drush_get_option('label', 'vcs'))],
    'ancestor-configuration_path' => $ancestor_configs[drush_get_option('ancestor-label', drush_get_option('label', 'vcs'))],
    'source-commit' => drush_get_option('source-commit', 'HEAD'),
    'target-commit' => drush_get_option('target-commit', 'HEAD'),
    'ancestor-commit' => drush_get_option('ancestor-commit', FALSE),
    'ancestor-site' => drush_get_option('ancestor-site', FALSE),
    'temp' => drush_get_option('temp', TRUE),
  );

  //STEP 1: create a temp repo
  $result['GITREPO_CLEAR'] = drush_shell_cd_and_exec("/tmp", 'rm -rf /tmp/site-merge');
  $result['GITREPO_BUILD'] = drush_shell_cd_and_exec("/tmp", 'git init /tmp/site-merge');

  //STEP 2: get the ancestor code in

  //go back in time and get the code for the source config before everything diverged
  $result += _drush_site_merge_copy_config($merge_info['ancestor-configuration_path'], $merge_info['ancestor-commit'], 'ANCESTOR');

  // Now create a branch to indicate the site moved on.
  $result['TARGET_BRANCH'] = drush_shell_cd_and_exec("/tmp/site-merge", 'git branch target_site HEAD');

  //STEP 3: update the code for the SOURCE config
  $result += _drush_site_merge_copy_config($merge_info['source-configuration_path'], $merge_info['source-commit'], 'SOURCE');

  //STEP 4: update the code for the TARGET config
  $result['TARGET_CHECKOUT'] = drush_shell_cd_and_exec("/tmp/site-merge", 'git checkout target_site');
  $result += _drush_site_merge_copy_config($merge_info['target-configuration_path'], $merge_info['target-commit'], 'TARGET');

  //STEP 5: Merge the two configs.
  // finally, merge the code from target into master, creating a code base where
  // the changes in site are preferred over the changes in master, and identifying
  // conflicts where changes happened in two locations.

  $result['TARGET_SWITCH'] = drush_shell_cd_and_exec("/tmp/site-merge", 'git checkout master');
  $result['TARGET_MERGE'] = drush_shell_cd_and_exec("/tmp/site-merge", 'git merge target_site');


  if($result['TARGET_MERGE']) {
    $result['MAINREPO_CLEAR'] = drush_shell_cd_and_exec($merge_info['target-configuration_path'], 'find . -path ./.git -prune -o -exec rm -rf {} \; 2> /dev/null');
    //TODO: I guess it's safe to clear this folder before copying to ensure that deletes take place. for now though, let the user perform the copy
    //$result['MAINREPO_COPY'] = drush_shell_cd_and_exec("/tmp/site-merge", 'cp -r . %s', $merge_info['target-configuration_path']);
  }
  else {
    //TODO: implement the --continue option
    drush_set_error("MERGE_CONFLICTS", "Merge conflicts were detected. Please resolve this in /tmp/site-merge, then run this command with --continue");
  }
  return $result;
}

function _drush_site_merge_copy_config($configpath, $commit = 'HEAD', $debugname = '') {
  $result[$debugname.'_CLEAR'] = drush_shell_cd_and_exec("/tmp/site-merge", 'find . -path ./.git -prune -o -exec rm -rf {} \; 2> /dev/null');
  $result[$debugname.'_COPY'] = drush_shell_cd_and_exec($configpath, 'git archive --prefix=/ %s ./ --format=tar | tar zxf - -C /tmp/site-merge/', $commit);
  $result[$debugname.'_COMMIT'] = drush_shell_cd_and_exec("/tmp/site-merge", 'git add -A .;git commit -m \'latest %s source code\'', $debugname);

  return $result;
}

function drush_site_merge_site_merge_rollback() {
  $merge_info = drush_get_context('DRUSH_CONFIG_MERGE_INFO');
}

