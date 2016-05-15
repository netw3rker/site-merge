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

  $items['site-merge'] = array(
    'description' => 'Merge configuration data from two sites or environments.',
    'aliases' => array('sm'),
    'arguments' => array(
      'site' => 'Source Configuration',
      'label' => "A config directory label (i.e. a key in \$config_directories array in settings.php). Defaults to 'sync'",
    ),
    'options' => array(
      'base' => 'The commit hash or tag for the base of the three-way merge operation.  This should be the most recent commit that was deployed to the site specified in the first argument.',
      'ancestor' => 'The location in the git history where the last successful merge took place',
    ),
    'examples' => array(
      'drush site-merge [sourcelocation] [targetlocation] --ancestor=C1B2DF' => 'Merge configuration changes from the default site with the configuration changes made on the target site.',
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
 * Implements drush config-merge command
 *
 * @param $alias
 *   The target site to merge configuration with
 * @param $config_label
 *   Which configuration set (active, etaging, etc.) to operate on
 */
function drush_site_merge_site_merge($alias = '', $config_label = 'sync') {

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

  //STEP1

  drush_shell_cd_and_exec("/tmp", 'git init ./site-merge');


  //return $result;
}

/**
 * If drush_config_merge() exits with an error, then Drush will
 * call the rollback function, so that we can clean up.  We call
 * the cleanup function explicitly if we exit with no error.
 */
function drush_site_merge_config_merge_rollback() {
  $merge_info = drush_get_context('DRUSH_CONFIG_MERGE_INFO');
  _drush_config_extra_merge_cleanup($merge_info);

  // If we messed with the commits on the original branch, then we need to put them back
  // the way they were if we roll back.  We don't want to do this on an ordinary cleanup, though.
  if (isset($merge_info['undo-rollback'])) {
    drush_shell_cd_and_exec($configuration_path, 'git reset --hard %s', $merge_info['merge-base']);
    drush_shell_cd_and_exec($configuration_path, 'git merge %s', $merge_info['undo-rollback']);
  }
}

/**
 * If the user wants to abandon the work of their merge, then
 * clean up our temporary branches and return TRUE to cause
 * the calling function to exit without committing.
 */
function _drush_site_merge_merge_action_abandon(&$merge_info) {
  _drush_config_extra_merge_cleanup($merge_info);
  drush_log(dt("All changes erased."), 'ok');
  return TRUE;
}

/* Helper functions */

/**
 * Reset our state after a config-merge command
 */
function _drush_site_merge_merge_cleanup($merge_info) {
  if (!empty($merge_info) && !empty($merge_info['configuration_path'])) {
    $configuration_path = $merge_info['configuration_path'];
    // If we are in the middle of a rebase, we must abort, or
    // git will remember this state for a long time (that is,
    // you can switch away from this branch and come back later,
    // and you'll still be in a "rebasing" state.)
    drush_shell_cd_and_exec($configuration_path, 'git rebase --abort');
    // Violently delete any untracked files in the configuration path
    // without prompting.  This isn't as dangerous as it sounds;
    // drush config-merge refuses to run if you have untracked files
    // here, and you can get anything that Drush config-merge put here
    // via `drush cex` (or just run config-merge again).
    drush_shell_cd_and_exec($configuration_path, 'git clean -d -f .');
    // Switch back to the branch we started on.
    $result = drush_shell_cd_and_exec($configuration_path, 'git checkout %s', $merge_info['original-branch']);
    if (!$result) {
      drush_log(dt("Could not return to original branch !branch", array('!branch' => $merge_info['original-branch'])), 'warning');
    }
    // Delete our temporary branches
    if ($merge_info['autodelete-live-config']) {
      drush_shell_cd_and_exec($configuration_path, 'git branch -D %s 2>/dev/null', $merge_info['live-config']);
    }
    if ($merge_info['autodelete-dev-config']) {
      drush_shell_cd_and_exec($configuration_path, 'git branch -D %s 2>/dev/null', $merge_info['dev-config']);
    }
  }
}
