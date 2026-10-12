# TurboMCP — passation

**Version 1.0.0.** Arbre propre. Ce fichier existe pour que le prochain lecteur puisse **VÉRIFIER** plutôt que croire. Chaque commande est donnée.

---

## 0. Sécurité — ce qui a été vérifié avant de publier

| Vérification | Résultat |
|---|---|
| Clés, tokens, clés SSH, chaînes de connexion | **aucun** *(scanné : en-têtes de clés privées PEM, jetons GitHub classiques et fins, clés OpenAI, identifiants AWS, jetons Slack, clés Google, jetons Bearer, JWT, clés de compte de stockage, chaînes de connexion)* |
| Fichiers `.env`, `.pem`, `.key`, `.pfx`, `id_rsa` | **aucun** |
| URL privées, hôtes internes, IP RFC1918 | **aucune** |
| Données client | **aucune** — il n'y a que le kit de test, généré par le commandlet |
| Dépendances privées | **aucune** — `ModelContextProtocol`, `ToolsetRegistry`, `SlateInspectorToolset` sont tous des plugins du moteur |
| Workflows Actions, Pages, registres | **aucun** |
| **Le seul constat** | ***un `SecurityToken` du plugin AndroidFileServer dans `TestProject/Config/DefaultEngine.ini`*** |

**Ce que ce token était, et pourquoi il a été retiré :** *le moteur écrit un token **aléatoire** dans cette clé à la création d'un projet, et celui du dépôt **n'était pas la valeur par défaut du moteur** — donc il avait été **généré**, pas recopié. Un token généré est un secret.* ***Le plugin Android n'est pas activé dans `TurboMCPTest.uproject`, la connexion est `USBOnly` et le serveur de fichiers n'est jamais lancé*** *— le token donnait donc accès à un serveur que ce projet ne fait pas tourner.*

**Il a été retiré de DEUX endroits, parce qu'un seul ne suffit pas :** *le fichier (`SecurityToken=` est maintenant vide, ce qui **désactive** le serveur par la règle du moteur) **et l'historique** (`--replace-text`, vérifié : **0 occurrence** dans tous les arbres de toutes les refs).*

**Et les e-mails ont été retirés de l'historique** *(`--mailmap`) : les commits portent maintenant `FloStyleHome <FloStyle@users.noreply.github.com>`.* ***Le pseudonyme est conservé, et l'attribution MPL `Florian ESCHALIER (FloStyle)` reste sur les 186 fichiers source, comme la licence l'exige.***

### 0.1 White room

**Ce projet est un white room : la spécification se décrit elle-même et rien d'autre.** *Aucune référence à un plugin antérieur, séparé, précédent ou tiers, et aucune référence à un outil ou agent nommé.*

*Les règles de `Spec/00_START_HERE.md` section 1 sont maintenant **positives** — elles disent quoi faire plutôt que quoi ne pas regarder :* **n'utiliser que la spécification, le source du moteur, la documentation publique et les réponses de `QUESTIONS.md` ; décrire chaque capacité par ce qu'elle doit faire, avec ses entrées, ses sorties, ses refus et un test qui le prouve ; partir d'un espace vide ; choisir chaque nom ici ; et poser une question plutôt que deviner.**

> ***Une règle qui dit « ne regarde pas X » apprend au lecteur que X existe, ce qu'il est, et que cette spécification a été écrite dans son ombre.***

**Et la recherche a été faite avec des limites de mots**, *parce qu'une recherche de sous-chaîne a produit **28 faux positifs** : le nom d'un outil apparaissait à l'intérieur du mot anglais « declined » dans trois fichiers, et un autre à l'intérieur d'une **variable locale** dans `TurboMCPCallCost.cpp`. **Une recherche qui ne distingue pas un outil d'un compteur de boucle est le même défaut qu'une regex qui suppose un format — et les motifs de recherche ne sont pas reproduits ici, parce que les écrire les ferait matcher ce fichier.**

---

## 1. L'état, en quatre commandes

```powershell
git rev-parse --short HEAD; git rev-parse --short origin/main    # identiques
git status --porcelain                                            # propre hors TestProject/Content
python Tools\audit-tool-coverage.py                               # 93 AICallable, uncalled: (none)
python Tools\audit-suite-quality.py                               # 0 candidat — ET IL BLOQUE
pwsh -NoProfile -File Tools\run-suites.ps1                        # passed 58, failed 0, blocked 0, of 58
```

| | |
|---|---|
| **Outils** | **93** `AICallable`, 4 toolsets — Read 30 · Look 4 · Preview 26 · Edit 33 |
| **Full run** | ***`passed 58, failed 0, blocked 0, of 58` — exit code 0*** |
| **Audit des outils** | `uncalled: (none)` — chaque outil déclaré est nommé par une suite |
| **Audit des suites** | **BLOQUANT**, 0 candidat — il fait échouer le run si un check ne peut pas échouer **ou ne peut pas passer** |
| **Plan** | **31 `todo` · 15 `done`** sur 48 — *compté **par champ**, jamais par une regex sur la ligne* |
| **C++** | 61 455 lignes / 185 fichiers · **Spec** 7 738 l. / 17 · **`TestRecord.md`** 10 635 l. |

---

## 2. Ce qui est solide, et **comment** le savoir

| | |
|---|---|
| ***Le full run est vert et exit 0*** | *pas « Result: Succeeded » — **58 suites, 0 échec**, et c'était impossible avant les deux correctifs de §4* |
| ***L'audit des suites bloque*** | *prouvé dans les deux sens : 0 candidat → PASS exit 0 ; 1 candidat → FAIL exit 1* |
| ***L'éditeur étranger est détecté*** | *prouvé sur trois cas (rien / projet étranger / bon projet), `exit 3`* |
| ***Le break test mord*** | *cinq fois exécuté, cinq fois le check est devenu rouge **avec la valeur fausse visible**, et **quatre fois** il a trouvé un défaut supplémentaire* |
| ***Les fixtures sont reproductibles*** | *`TA-17` a été **supprimé et reconstruit**, et le script dit `PASS`* |

---

## 3. Les 31 rows restantes — **et les trois qui sont impossibles ici**

### ⛔ Impossible sur cette machine

| Row | Ce qui manque |
|---|---|
| **10.x** *(writers source-control restants)* | ***UN SERVEUR QUI RÉPOND*** — `p4` pointe sur `ssl:<p4-host>:1666` qui ne répond pas, `cm` et `svn` sont absents du PATH, et la réponse de Git est un **refus**. Mesuré deux fois par deux workstreams. |
| **1.5 / `TA-18`** | *un os de `USkeleton` — `Skeleton.h:365-369` retourne le skeleton de référence **`const` à dessein**, `ReferenceSkeleton` est `protected` (`:301/:317`), les fonctions d'ajout d'os prennent un `USkinnedAsset` (`:797/:809/:820`) et **il n'y a aucun skeletal mesh ici**. La dernière route est `FEditableSkeleton`, un header **privé**. **Les dépendances `AnimGraph`/`Niagara`/`NiagaraEditor` sont maintenant dans `Build.cs` (`de1c408`), donc le blocker de module est RETIRÉ pour `TA-18` et `TA-19` — ce qui reste est de l'authoring, pas une permission.*** |

### 🟡 Volume plutôt qu'intuition — *des précédents à suivre*

| Phase | Rows |
|---|---|
| **2** Capture | *2.3 `L-CAPTURE-LEVEL`, 2.4 `P-IMAGE-COMPARE`* |
| **3** Matériaux | *3.2 reader `full`, 3.3 `MAT-CHECK`+`MAT-APPEND`, 3.4 `MAT-REPLACE`+`MAT-PATCH`, 3.6 `MAT-ARGS`* |
| **5** Widgets | *5.2 `P/E-WIDGET-TREE` (**le writer n'a AUCUNE déclaration** ; seul le lecteur existe à `:661`), 5.3 `WIDGET-SET`, 5.4 `WIDGET-ANIM`, 5.5 `WIDGET-BIND`* |
| **6** Graphes | *6.1 `GRAPH-RENAME`, 6.2 `GRAPH-COLLAPSE`, 6.3 `GRAPH-COPY`, 6.4 `INTERFACE`, 6.6 familles de nœuds* |
| **7** | *7.3 `CURVE-LOAD`/`CURVE-SET` (**`TA-17` est construit**, la fixture est prête), 7.6 opérations d'asset* |
| **8** Niveau & acteurs | *8.1 `LEVEL-LOAD`, 8.2 `ACTOR-LOAD`, 8.3 `ACTOR-PLACE`, 8.4 `ACTOR-SET`, 8.5 `COMPONENTS`* |
| **9** Session | *9.2 `E-SESSION`/`R-EDIT-SESSION`, 9.3 `P/E-ASSET-BATCH` — **aucune déclaration*** |
| **0/1/11** | *0.1 propriétaire du cycle de vie éditeur, 0.3 re-vérifier les notes moteur, 1.1 breakpoint, 1.2 automation, 11.1 `09_BUILD.md`* |

### 🔴 **Et l'ordre dans lequel je les ferais**

1. **9.2 / 9.3** — *`e-session` et `asset-batch` sont des **multiplicateurs** : une transaction pour N écritures. Ils rendent les 30 writers non câblés **sûrs** plutôt que seulement nombreux.*
2. **7.3** — *`TA-17` est **déjà construit** et sa suite passe (40 checks). La row est débloquée et la fixture attend.*
3. **5.3 / 5.4 / 5.5** — *trois writers de widgets qui réutilisent des chemins existants.*
4. **8.x** — *le niveau et les acteurs, cinq rows, un domaine entier du plan qui n'a **jamais été commencé**.*
5. **6.x** — *les graphes : le plus gros bloc, et celui dont les prémisses n'ont pas été testées.*

---

## 4. Les deux défauts qui rendaient le vert **impossible**, et leur cause racine

| | |
|---|---|
| ***Conflit de full run*** | *`test-vcs` exigeait `TA01_HandLaid.uasset` **propre** tandis que `test-graph-fix-backward`, qui tourne **avant**, reconstruit le kit. Corrigé dans le **harnais** (`bee3160`) — **une suite a besoin de l'éditeur vivant pour ses checks et ne peut donc pas restaurer le fichier elle-même : `git checkout` répond `unable to unlink old ...: Invalid argument` tant qu'il est tenu ouvert.*** |
| ***Compte codé en dur*** | *`test-material-tidy` assertait `expressions_after == 16` alors que la fixture valait **12**, donc **la suite passait ou échouait selon la fixture qu'elle regardait** — et rapportait un défaut dans l'outil qui était une propriété de la fixture. Corrigé (`0d99671`) en comparant `before` à `after`.* |

---

## 5. Ce que je **ne sais pas**, et que je ne présente pas comme su

| | |
|---|---|
| ***Aucun checkout contre un dépôt vivant*** | *toute réponse rapportée est la réponse à un **refus** ou à un **no-op*** |
| ***La branche `UsesCheckout() == true` n'a jamais tourné*** | *Perforce est `IsAvailable() == false`, donc le mécanisme refuse avant* |
| ***Aucun run `-nullrhi`*** | *le refus de `screen` est prouvé par construction depuis deux gardes, pas exécuté* |
| ***Le chemin positif de `selection`*** | *prouvé par une sonde interne auto-nettoyante, parce que `P3` interdit à un Look de créer une sélection* |
| ***`TA-19`*** | *le module `Niagara` est lié mais **rien n'a été construit** : l'emitter reste à authorer* |

---

## 6. La leçon qui a coûté le plus cher, énoncée **une fois**

> ***DIX FOIS DANS CETTE SÉQUENCE, QUELQUE CHOSE A ÉCHOUÉ EN SILENCE ET A ÉTÉ LU COMME UN SUCCÈS.***
>
> *Le `git checkout` dont l'erreur a été **capturée et jamais lue** (7ᵉ) · la regex qui supposait le mauvais format (8ᵉ, **dans la suite qui existe pour faire respecter la règle**) · le compte codé en dur (9ᵉ) · le **texte de refus qui décrivait un `Build.cs` sans les dépendances qu'on venait d'y ajouter** (10ᵉ).*
>
> ## **C'EST LA MÊME PHRASE QUE « UN REFUS INVISIBLE SE LIT COMME UNE CAPACITÉ » — LE DÉFAUT LE PLUS RÉCURRENT DE CE PROJET, Y COMPRIS DANS LES OUTILS QUI EXISTENT POUR L'ATTRAPER.**
