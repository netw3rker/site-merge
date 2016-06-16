
Requirements
------------
* Works with Drush master (Drush version 8.x) and Drupal 8.0.x and later.
* Requires a three-way merge tool, such as kdiff3

Installation
------------

1. cd "$HOME/.drush"
2. composer global require "netw3rker/site-merge"
3. drush cc drush

Usage
-----

This tool is designed to work with Drupal 8 installations that work in
multisite mode, where each site descends from a common (and maintained)
ancestor site. The configuration for each site is expected to be exported
and tracked in individual folders within the same repository. For example:

The default site "example.com" provides content about cities.
a specific site "nyc.example.com" provides content only about NYC.
a specific site "sfo.example.com" provides content only about SFO.

These three sites exist as separate configuration sets with separate 
databases of content. The only important aspect of this is that the NYC
and SFO sites were initially created by importing the default example.com
configuration set first.
 
The goal of the maintainer is to be able to make configuration changes 
to example.com, and then merge those changes into SFO and NYC sites. The 
merge should preserve any non-conflicting changes between the sites.

The desired functionality can be easily achieved with this tool by running
the following command:

drush site-merge default sfo.example.com --ancestor-commit=[HASH]




Resources
---------
