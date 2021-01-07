# Thcrap documentation

This project always lacked documentation. The 3 main sources of documentation are:
- Code comments.
- Ask someone who knows.
- Read the code to see what it does.
This document will try to fix that. Be careful about it says: some parts of it will become outdated, we _will_ forget to update it. But a lot of parts should remain the mostly the same. For example, I don't think we will ever stop having distinct patches.

This documentation will have 2 main parts: The files and The code. But before we dig into them, let's start with some glossary and core concepts.

## What are we
We are mostly 2 "things": thcrap and thpatch. 
thcrap, or Touhou Community Reliant Automatic Patcher (you don't need to remember the full name, nobody use it), is a patching tool for Touhou games. You run a Touhou game with thcrap, and thcrap will modify it to apply all kind of patches to it. It will translate its texts to English, French or Klingon, it will let you play Sakuya or Mima in the games where ZUN forgot to put them, or it will replace the game you wanted to play with another game that happen to use the same engine. These patches are made either by us or by the community. 
thpatch, or Touhou Patch Center (do remember this full name, everyone use it), is the website at https://thpatch.net/ , which is hosted by us. This is a custom MediaWiki with a translate extension and a lot of patches. It hosts most of our translation patches and provides an interface for everyone to contribute to the translations. "Touhou Patch Center" is also the name of our group.

## Patches and repositories
Our group is all about patching, so it makes sense that patches are the most important part of our software. 
A patch is a "pack of modifications" that can be applied to one or more games. The most popular example is the `thpatch/lang_en` patch: it changes the game texts to English. The `gamer251/mima` patch changes enough of a game to replace a playable character with Mima. If you use thcrap, that's usually because you're interested in one or more patches. 

### Patch stacking
You can apply several patches at the same time on a game. This is known as "patch stacking". For example, you can apply both `thpatch/lang_en` and `thpatch/lang_fr`. The French patch will be used when it can provide a translation, and the English patch will be used for things that haven't been translated in the French patch. You can use the `thpatch/lang_en` and `DTM/alphes` patches to have the texts translated to English *and* the character sprites replaced with Alphes-style art. And of course, you aren't limited to 2 patches. 
In fact, if you select only `thpatch/lang_fr`, you already get 6 patches:
- `nmlgc/base_tsa` and `nmlgc/base_tasofro`, which are needed most of the time.
- `nmlgc/script_latin`, which enhances things for languages using the latin alphabet (for example, it changes the font to one with a lot more characters).
- `nmlgc/western_name_order`, for languages that put the first name before the last name.
- `thpatch/lang_en`, for when a French translation isn't available, because we assume most French people can read English better than Japanese.
- `thpatch/lang_fr`, which is the patch you selected.
They are listed in reverse order of priority. When trying to patch something, thcrap will start at the bottom, and go up until it finds a patch that can provide a replacement.

### Repositories
Repositories, which is always abbreviated to "repos" because this is such a long word, are "group of patches made by one person/group". You may have notices that all the patches mentionned above follow the `something/something_else` naming convention. This is actually `repo_name/patch_name`. For example, the patch `thpatch/lang_en` is the patch `lang_en` in the repo `thpatch`. 
We have 2 repositories: nmlgc, which contains all the patches we host on Github, and thpatch, which contains all the patches generated from the Touhou Patch Center website. All the other repositories (like gamer251 and DTM which I mentionned before) are owned by other people, and host these people's patches. If you want to make you own patch, you probably want to make you own repo. 
Btw, when we talk about patches, we tend to omit the repo name. For example, we talk about `base_tsa` or `lang_en` instead of `nmlgc/base_tsa` or `thpatch/lang_en`. All the `lang_*` patches + the skipgame patches are in the `thpatch` repo, and all the other ones from us are in the `nmlgc` repo (nmlgc is the founder of thcrap, so the split used to be "patches from nmlgc" / "patches from people who use the thpatch website", but when more people joined the team, it made more sense to keep a single Github repository for all of us, and the name stuck).

## Let's start
That's all for the introduction, let's go to the next part!
1. The files
2. The code
