Patcheur automatique de la communauté Touhou
--------------------------------------------

### Description ###

En gros, ceci est un framework customisable et améliorable pour patcher des appliquations Windows, concu en particulier pour la traduction de jeux japonais.

Ce framework est développé pour faciliter l'auto mise à jour, la traduction en plusieurs langues des jeux [Touhou Project] (http://en.wikipedia.org/wiki/Touhou_Project) sur [Touhou Patch Center] (http://thpatch.net/), mais peuvent être utilisés théoriquement pour n'importe quel autre patch de ces jeux, sans passer par ce site.

#### Fonctions principales du moteur de base #####

* **Injections DLL** facile du moteur principal et des plug-ins dans le processus cible.

* **Propagation complète vers les processus fils**. Cela permet l'utilisation d'autres patchs externes qui utilisent aussi des injections de DLL, avec thcrap. (Oui, ça a essentiellement été développé pour vpatch.)

* Utilise **JSON pour toute les données de configuration de patch**, rendant les patchs open-source. En fusionnant les objets JSON de façon récursive, cela nous donne...

* **Accumulation de patchs** - Appliquez n'importe quel nombre de patch à la fois, triés par ordre de priorité. Permet la mise en liste noir de fichiers à l'aide de jokers dans certain patchs lors de la configuration.

* Automatically adds **transparent Unicode filename support** via Win32 API wrappers to target processes using the Win32 ANSI functions, without the need for programs like [AppLocale] (http://en.wikipedia.org/wiki/AppLocale).

* Les patchs peuvent gérer **plusieurs builds et versions** d'un même programme, identifié par une combinaison de hachage en SHA-256 et de tailles de fichier .EXE.

* **Binary hacks** pour les modifiquations arbitraires de la mémoire interne du programme original (utilisé surtout pour des assemblages customisés).

* **Points d'arrêt** Pour appeller des fonctions DLL customisées à n'importe quel instruction du code original. Ces fonctions peuvent lire et modifier l'état actuel du registre CPU.

 * **Points d'arrêt fichier** pour remplacer les fichiers en mémoire depuis ceux des patchs.

* Wildcard-based **crochet de patching de format de fichier** appellés lors du remplacement de fichier - peut appliquer les patchs au fichiers selon une description JSON (accumulable!).

* ... Et tout ça sans aucun impact signficatif sur les performances. ☺

### Modules inclus ###

* `win32_utf8`: une bibliothèque de wrapper UTF-8 autour de l'API Win32 dont nous avons besoin. C'est un projet stand-alone qui peut (et devrait) être utilisé librement dans tout autre application.
* `thcrap`: Le moteur principal de patch
* `thcrap_loader`: Un chargeur de ligne de commande pour appeller les fonction d'injection de `thcrap` sur un processus récemment créé
* `thcrap_configure`: un petit utilitaire de configuration de patch à lignes de commandes. Sera à terme remplacé par un outil à interface graphique.
* `thcrap_tsa`: un plug-in thcrap contenant des hooks de patch pour les jeux utilisant le moteur STG de Team Shanghai Alice.
* `thcrap_update`: un plug-in de thcrap contenant des fonctionalités de mise à jour pour les patchs

### Compilation ###

Jusqu'à maintenant, tout les sous-projets ne sont constitués que d'un fichier de projet Visual C++ 2010 pour la compilation. SP1 est recommandé, ne serait-ce que pour une identifiquation correcte de la version dans l'explorateur Windows. Des configurations de compilation pour d'autres systemes d'exploitation sont toujours les bienvenues.

#### Dépendences ####

* [Jansson] (http://www.digip.org/jansson/) est nécessaire pour tout les modules sauf `win32_utf8`. Compilez le à partir de la [dernière source] (https://github.com/akheron/jansson), et ensuite ajoutez ses includes et dossiers de bibliothèques à chaque projet.

* [libpng] (http://www.libpng.org/pub/png/libpng.html) **(>= 1.6.0)** est requis par `thcrap_tsa` pour le patching d'images.

* [zlib] (http://zlib.net/) is required by `thcrap_update` for CRC32 verification. It's required by `libpng` anyway, though.

* The scripts in the `scripts` directory are written in [Python 3] (http://python.org/).

### Licence ###

Le patcheur automatique de la communaute Touhou francophone et traduit par Touhou-Online, et tous les modules l'accompagnant sont la propriété de Touhou-Online ([TO] (http://www.touhou-online.net)), sauf mention contraire. Cela devrait éliminer toute confusion possible à propos des termes de licence, ainsi que toute question.

Cette licence est publique. La communauté Touhou nous octroie l'opportunité de disposer d'un tel outil, et nous nous permettons donc d'apposer notre propre licence sur cet outil suite à l'accord de la licence d'origine.

Nous avons choisi une licence plus stricte, afin de faire respecter cette licence et cet outil au sein de la communauté francophone. Cependant, si vous souhaitez effectuer les modifications, cela ne devrait pas poser problème, tant que vous respectez les conditions.

Ceci dit, nous ne tenons pas à ce que notre ego vous empêche de faire des trucs geniaux avec tout ça. Repacker un build customisé avec de beaux artworks faire de la pub pour votre propre comunauté? Génial ! Réarranger le projet pour se focaliser sur la traduction de visual novel via serveur privé? Génial! Changer de larges portions du code par vous même, créant ainsi un projet supérieur et plus abouti? Génial, il se pourrait même que nous supportions votre projet à la place du notre!

Cela dit, nous apprecions *quand même* les attributions ☺

### Contribuer ###

Ce projet est en dévelopement actif, et les contributions sont toujours les bienvenues.

Nous suivons le statut du dévelopement de futures fonctions de ce projet sur la page de GitHub de thpatch.net associée. N'hésitez pas à intervenir, discuter des détails d'implémentation, et à contribuer au code - nous apprécierions *vraiment* pouvoir partager la charge de travail avec plus de développeurs.

Si, pour une raison quelquonque, vous ne voulez pas passer par GitHub, vous pouvez aussi contacter Nmlgc par mail à nmlgc@thpatch.net. Vous pouvez aussi contacter NightLunya par mail à nightlunya@touhou-online.net, pour d'autres requêtes.

### Dévelopement de Plug-in ###

A cette étape, nous vous recommandons de nous envoyer régulierement des requêtes pour n'importe quel plug-in que vous développez autour du moteur de base. l'API du moteur de base est loin d'être finie et peut être sujette à des modifications. Dans le cas où nous y changerions quelque chose, ces changements peuvent directement affecter votre projet.
