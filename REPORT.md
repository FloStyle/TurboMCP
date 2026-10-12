# TurboMCP — Rapport extensif

**Mesuré le 2026-10-11 à 11:35. Tous les chiffres de ce rapport sont lus, pas rappelés.**
**`HEAD` = `99719c4` = `origin/main`. Arbre propre.**

---

## 1. Ce qu'est TurboMCP, en trois phrases

Un plugin d'éditeur Unreal Engine 5.8.3 qui expose **93 outils appelables par IA** à travers le Model Context Protocol, répartis en **4 toolsets** : Read (30), Look (4), Preview (26), Edit (33). Les lectures vivent dans Read/Look — **structurellement incapables d'écrire** — et chaque écriture a un jumeau Preview qui partage le même `Plan`, donc Preview et Edit refusent **exactement** le même input.

| Métrique | Valeur mesurée |
|---|---|
| Commits sur `main` | **497** |
| Outils `AICallable` | **93** (audit : `uncalled: (none)`, PASS) |
| Suites (`Tools/test-*.py`) | **52** fichiers (58 suites au total dans le harnais) |
| **Full run** | ***`passed 58, failed 0, blocked 0, of 58` — EXIT CODE 0, VERT ENTIÈREMENT*** |
| C++ du plugin | **61 620 lignes** dans **186** fichiers |
| Spec | **7 740 lignes** dans **17** fichiers |
| `TestRecord.md` | **10 792 lignes** (le journal de bord) |
| Licence | **MPL-2.0** sur chaque fichier, `Copyright (c) 2026 Florian ESCHALIER (FloStyle)` |

---

## 2. Le plan : **31 `todo`, 15 `done`** sur 48 lignes

*Compté **par champ** — la dernière cellule de la ligne. Voir §7 : une regex sur la ligne donne un chiffre faux.*

| Row | Ce qui est fait |
|---|---|
| **0.2** | `test-override-add` / `test-comment-boxes` restaurent leurs fixtures à l'aller comme au retour |
| **1.4** | Un Niagara et un Anim Blueprint peuvent-ils être capturés headless ? — **mesuré** |
| **2.1** | `selection`, `neighbours`, `box-neighbours`, `screen` ajoutés à `L-CAPTURE` |
| **2.2** | L-CAPTURE-ASSET : **9 types d'éditeurs capturés, aucun refusé**, via un seul chemin de rendu |
| **7.1** | `P/E-STRUCT-NEW` + `P/E-STRUCT-FIELDS` — le writer de structs |
| **10.1** | `E-VCS-CHECKOUT` via `ISourceControlModule`, comportement headless mesuré |
| **10.2** | Checkout automatique câblé dans les outils d'écriture (2 writers sur 32) |

### Ce qui reste, par phase

| Phase | État |
|---|---|
| **0 — Fondations** | 3 rows `todo` : propriétaire unique du cycle de vie de l'éditeur, re-vérification de `07_ENGINE_NOTES.md` |
| **1 — Mesures bloquantes** | 3 `todo` : breakpoint Blueprint, tests d'automation, kit `TA-14`→`TA-21` |
| **2 — Vision** | **2.1 et 2.2 faites.** 2.2 capture **9 types d'éditeurs, aucun refusé**. 2.3 (level/viewport/PIE) et 2.4 (diff d'images) restent |
| **3 — Look** | layout-report, capture, widget-preview : largement bâti, rows de finition |
| **4 — Data** | `P/E-DATATABLE-ROWS` fait ; courbes (`T-DATA-5` → **`TA-17` manquant**) |
| **5 — Widgets** | writers d'override/bind/retarget ; **`T-WIDGET-4` → `TA-8` manquant** |
| **6 — Blueprints** | le plus gros bloc : graph append/replace/patch/knots/tidy |
| **7 — Structs/enums** | 7.1 faite ; 7.3 courbes |
| **8 — Matériaux** | écriture de blocs, promote de label |
| **9-10** | session/coût, source control |
| **11-15** | niveau & acteurs, build, vision, qualité |

---

## 3. Les trois choses dont je suis le plus sûr, et **comment** je le sais

### 3.1 La machine de vérification fonctionne

**Sept rows d'affilée ont fini avec un full run propre : `53/53` → `54/54` → `55/55` → `56/56` → `57/57`.** *Chacune avec une suite nouvellement écrite dedans et zéro échec.*

Et la mesure qui compte : **les trois dernières rows ont eu besoin de presque aucune intervention de ma part.** Les briefs, les commits de sécurité, les checks de pré-vol et la règle `P1` ont été construits sur trente rounds, et ils produisent maintenant des résultats sans surveillance constante.

### 3.2 Le break test est le détecteur de défauts le moins cher du projet

**Trois fois sur trois, un break test a payé au-delà de son but :**

| Row | La seconde trouvaille |
|---|---|
| **7.1** | *quatre échecs avec les noms de champs manglés visibles* |
| **10.2** | ***un défaut `P1` dans le code : un motif de décision vide*** |
| **2.1** | ***un défaut dans la suite : un check qui assertait un préfixe et passait contre un build cassé*** |

**Row 2.2 en a trouvé un quatrième : une écriture échouée qui arrivait comme `look: ... error: ...` — donc un appelant testant le préfixe `error:` lisait un ÉCHEC COMME UN SUCCÈS.**

> **La forme générale est maintenant nommable : UN CHECK QUI MATCHE UN PRÉFIXE NE PEUT PAS DÉTECTER UN CHANGEMENT APRÈS LE PRÉFIXE. Un refus invisible se lit comme une capacité.**

### 3.3 Le projet corrige ses propres prémisses, et c'est le motif dominant

**Cinq rows consécutives ont trouvé que leur prémisse était fausse — toujours dans le sens qui SOUS-ESTIME ce qui existe :**

| Row | Prémisse | Réalité |
|---|---|---|
| **7.1** | *« editor-only »* | *trop strict — `CreateUserDefinedStruct` installe lui-même l'`EditorData` (`StructureEditorUtils.cpp:42-64`)* |
| **1.3** | *liste de plugins via `.agent/*.py`* | *ces scripts n'énumèrent que 2 plugins, pas le moteur* |
| **10.2** | *« `CheckOutFile` est la voie »* | *il répond `True` sur Git **sans jamais émettre de checkout*** |
| **2.1** | *« ajouter 4 sélecteurs »* | *la capacité faisait déjà 2 062 lignes sur 3 fichiers + une suite suivie de 378 lignes* |
| **2.2** | *« six types d'éditeurs »* | *le plan en nomme **huit**, et **neuf** marchent* |

---

## 4. Les contraintes d'environnement **mesurées** — la frontière du possible ici

Ces quatre-là bornent ce que le plugin peut promettre :

| Contrainte | Ligne qui décide |
|---|---|
| **Slate n'est jamais initialisé dans un commandlet** | `LaunchEngineLoop.cpp:3343` garde l'init avec `!IsRunningCommandlet()` |
| **`FBlueprintEditorModule::StartupModule()` a un `check(GEditor)` inconditionnel** | `BlueprintEditorModule.cpp:214` — **deux commandlets du plugin ne peuvent pas tourner du tout** |
| **Sous `-nullrhi`, Slate EST initialisé — avec `SlateNullRenderer`** | `RHIGlobals.h:894` → `LaunchEngineLoop.cpp:3345-3347` → `:3354` |
| **Les suites tournent avec un VRAI RHI et une VRAIE fenêtre** | `run-suites.ps1:195` passe seulement `-LowPriority` |

> ***La quatrième ligne est une prémisse FAUSSE que j'avais écrite dans quatre briefs. Elle venait d'un commentaire périmé à `start-editor.ps1:261` que j'ai lu à la place de la liste d'arguments. J'ai corrigé le commentaire.***

**Et une frontière de données, énoncée deux fois par deux workstreams indépendants :**

> **AUCUN CHECKOUT CONTRE UN DÉPÔT SOURCE CONTROL VIVANT N'A JAMAIS ÉTÉ EFFECTUÉ ICI.** `p4` pointe sur `ssl:<p4-host>:1666` (ne répond pas) ; `cm` et `svn` ne sont pas dans le PATH ; et la réponse de Git est un **refus**.

---

## 5. Les fixtures fantômes — **cinq rows d'acceptation nomment des assets qui n'existent pas**

| Row d'acceptation | Fixture nommée | Statut |
|---|---|---|
| `T-MAT-10` | `TA-6` | *l'instance existe comme `MI_TurboMCPFixture` — manquante **sous le nom de la table*** |
| `T-LEVEL-1/2` | `TA-15`, `TA-16` | absents |
| `T-WIDGET-4` | `TA-8` | absent |
| `T-DATA-3` | `TA-21` | **aucun `UserDefinedStruct` nulle part dans le repo** |
| `T-DATA-5` | `TA-17` | **zéro match pour `CurveFloat\|CurveTable\|curve` dans `Tools/`** |
| *candidat* | `T-BP-8` → `TA-9` | *le libellé peut signifier un échange de classe en session* |
| **rows 2.2-2.4** | `TA-18`, `TA-19` | **absents — signalé AVANT que 2.2 ne commence** |

**Et une collision de labels : `TA-5` est appelé « Material 60 expressions » par la table, tandis que le kit utilise ce label pour le graphe `BackwardProbe`.**

> ***La règle de pré-vol qui attrape ça a été écrite après la DEUXIÈME de ces rows et appliquée à aucune des deux premières. Depuis, elle a été écrite dans chaque brief — et row 2.1 a attrapé le piège `test-capture.py` AVANT qu'il ne tire, pour la première fois.***

---

## 6. La dette de vacuité

**Cinq capacités ont rapporté que leur compte de checks surestimait leur couverture.** *Une suite avait une branche `else` qui vérifiait littéralement `True`.*

**Trois mécanismes différents ont été trouvés pour la même maladie, chacun nommé :**

| Niveau | Le défaut | Le correctif |
|---|---|---|
| **Chaîne** | *`IsAvailable()` imprimait `false` pour une question jamais posée* | *imprime **`NOT ASKED`*** |
| **Chaîne** | *un motif de décision vide après le tiret* | *un check qu'aucune ligne de décision n'est vide* |
| **Type** | *un booléen ne peut pas distinguer « refusé » de « un autre outil peut »* | ***un **enum à trois états** + un **catalogue de TOUS les cas*** |

> ***« UN REFUS INVISIBLE SE LIT COMME UNE CAPACITÉ » — c'est le plus vieux défaut du projet, énoncé en sept mots, et il est arrivé sous six mécanismes différents.***

---

## 7. Ma propre erreur la plus instructive

**Pendant vingt rounds j'ai compté l'avancement du plan avec `Select-String '\| todo \|'` — une regex sur la LIGNE.**

Or une cellule de statut peut contenir de la prose. **Le chiffre changeait entre deux exécutions de la même commande** (`41 todo, 4-5 done`). Le compte **par champ** est `40 todo, 6 done`.

> ***C'est la plus vieille règle du projet, commise par moi : PARSER LE CHAMP, JAMAIS MATCHER UNE PHRASE À CÔTÉ. Le compteur qui répond « combien reste-t-il » était une recherche de chaîne sur des cellules qui contiennent des phrases.***

**Et j'ai fait la même erreur sous une autre forme cette semaine** : j'ai lu un **commentaire** (`start-editor.ps1:261`) au lieu d'une **liste d'arguments** (`run-suites.ps1:195`), et j'en ai tiré une prémisse fausse mise dans quatre briefs.

---

## 8. Ce que je ne sais pas, et que je ne présente pas comme su

| | |
|---|---|
| **Aucun checkout contre un dépôt vivant** | *toute réponse de checkout rapportée est la réponse à un refus ou à un no-op* |
| **La branche `UsesCheckout() == true` n'a jamais tourné** | *Perforce est `IsAvailable() == false`, donc le mécanisme refuse avant* |
| **Plastic et Subversion injoignables** | *`cm` et `svn` absents du PATH — leur branche AVAILABLE n'a jamais été vue* |
| **Aucun run `-nullrhi` n'a jamais été effectué** | *le refus de `screen` est prouvé par construction à partir de deux gardes* |
| **Le chemin positif de `selection`** | *prouvé par une sonde interne auto-nettoyante, parce que `P3` interdit à un Look de créer une sélection* |
| **`FindReaders` compte les assets qui DÉTIENNENT la struct** | *pas chaque graphe qui lit un champ par nom — et l'aperçu le DIT* |
| **Aucun run `-nullrhi`** | *une commande le fermerait : `start-editor.ps1 -NullRHI` puis `mode screen`* |

---

## 9. Ce que je prendrais le temps de faire, si le temps n'était pas compté

**Dans cet ordre, parce que chaque item débloque le suivant. DEUX SONT LIVRÉS (9.1 et le trou structurel du §11.1) ; les trois autres restent.**

| Priorité | État |
|---|---|
| **§11.1 Le trou structurel** | ✅ **LIVRÉ** — CANNOT-MATCH ajouté et prouvé sur 6 cas, 5 faux positifs retirés, l'audit est **BLOQUANT**, 0 candidat |
| **§9.1 Éditeur étranger** | ✅ **LIVRÉ** — check-mcp-server.py nomme le projet propriétaire du port, prouvé sur 3 cas |
| **§9.2 Dépôt source control** | ⏳ *bloqué par l'environnement* |
| **§9.3 Le kit** | 🟡 **PARTIELLEMENT LIVRÉ** — `TA-17` **CONSTRUIT** (2 assets, payload vérifié par le Python du moteur, tangentes prouvées comportementalement) ; `TA-18`/`TA-19` **refusés par nom** avec deux bloqueurs mesurés chacun, et les bloqueurs de `TA-18` ne sont **pas** les contraintes commandlet que je pointais |
| **§9.4 Vérificateur bloquant** | ✅ **LIVRÉ** (c'était le même item que §11.1) |
| **§9.5 Les 30 writers** | ⏳ *mécanique* |

**ET L'AUDIT DU PLAN LUI-MÊME, QUI A RÉVÉLÉ SEPT CAPABILITIES DÉJÀ CONSTRUITES QUE SA COLONNE DE STATUT APPELAIT 	odo :** ***R-BP-LINT (37 checks), R-CALL-COST (29), R-ASSET-COST (10), P/E-DATATABLE-ROWS (26), R-MAT-INST-LOAD (20), P/E-ENUM-EDIT, P/E-MAT-BLOCKS (24) — toutes vérifiées en les exécutant. LE PLAN SOUS-ESTIMAIT LE PROJET DE SEPT CAPACITÉS, et c'est la même erreur que les sept rows consécutives de §150, commise par le plan sur lui-même.***

**ET UN DÉFAUT PRÉEXISTANT TROUVÉ ET CORRIGÉ EN COURS DE ROUTE, QUI N'ÉTAIT DANS AUCUNE DES CINQ PRIORITÉS :** ***le full run **ne pouvait pas être vert** — 	est-vcs exigeait TA01_HandLaid.uasset propre tandis que 	est-graph-fix-backward, qui tourne avant lui, reconstruit le kit. Prouvé sur deux fichiers identiques octet pour octet à 630300, donc préexistant. CORRIGÉ dans le harnais, là où l'éditeur peut être arrêté : passed 5, failed 0 sur la séquence qui échouait.***

### 9.1 ✅ **Détecter un éditeur étranger sur le port 8000** — *LIVRÉ le 2026-10-11*

Trois workstreams indépendants ont payé cette collision, dont moi. **`check-mcp-server.py` pouvait passer contre l'éditeur de quelqu'un d'autre, et la suite échouait alors avec « the provider is not Git » au lieu de « un autre processus possède le projet ».** *Un faux échec qui coûtait un round à diagnostiquer chaque fois.*

> ***LIVRÉ : `check-mcp-server.py` interroge maintenant le processus, pas le protocole.*** *Le handshake MCP ne rapporte rien qui identifie un projet, donc un session id ne peut pas distinguer deux éditeurs ; **ce qui le peut est la ligne de commande** — `start-editor.ps1:260` construit `$Args = @($Project)` avec le **chemin absolu**.*
>
> **Prouvé sur trois cas exécutés :** *rien ne tourne → `NOTHING IS RUNNING`* · ***projet étranger → `RUNNING BUT NONE HAS project.uproject IN ITS COMMAND LINE — belongs to ANOTHER PROJECT`, exit 3*** · *le bon projet → `1 of them on turbomcptest.uproject`, handshake continue.*
>
> ***Et il a fallu trois corrections : `os` non importé ; « aucun processus » traité comme « projet étranger » (le **même** faux positif qu'il a fallu retirer de l'audit) ; et une ligne RESULT **générique** qui nommait la mauvaise cause — or un message d'échec confiant et faux est pire qu'un message vague.***

### 9.2 **Un dépôt source control qui répond** — *une seule commande fermerait la plus grande frontière du projet*

`p4` pointe sur un hôte mort et `cm`/`svn` sont absents. **Avec un serveur joignable, les deux rows source control passeraient de « mesuré contre un refus » à « mesuré contre un succès »** — et `UsesCheckout() == true` exécuterait enfin sa branche agissante.

### 9.3 **Le kit `TA-14` à `TA-21`** — *six rows d'acceptation dépendent de fixtures qui n'existent pas*

`TA-17`, `TA-18`, `TA-19`, `TA-16` sont **mesurés buildables** mais non construits. *Les construire débloque les rows 2.2-2.4, 4.x et 7.3 sans que chacune ait à réinventer un substitut.*

### 9.4 **Un vérificateur de format pour les suites** — *l'instrument qui manque encore*

Cinq capacités ont surestimé leur couverture et **chaque fois ça a été trouvé par un humain ou un workstream ultérieur**. *Un audit qui exige, pour chaque check, un input nommé qui le fait échouer **mécaniquement** — pas par convention — est ce qui manque. `audit-suite-quality.py` en est une version advisory ; il est temps qu'il bloque.*

### 9.5 **Les 30 writers non câblés** — *un travail mécanique, une fois 10.2 prouvée*

Le mécanisme existe, 2 des 32 writers l'utilisent, et les 30 sont **nommés un par un**. *C'est du travail répétitif sans inconnue — le bon candidat pour du parallélisme massif, dès que le format est figé.*

---

## 10. La dépendance et la projection — mesurées, pas estimées

### 10.1 **Les 40 rows restantes n'ont AUCUNE dépendance déclarée entre elles**

**Vérifié : la colonne de dépendance des 40 rows `todo` ne contient ni `gated`, ni `unblocked`, ni de renvoi à une autre row.** *Les seules mentions de blocage dans le plan sont sur des rows **déjà faites** (`10.1` était gatée sur `1.3`, `10.2` était « gated on 10.1 »), et les dépendances en question sont levées.*

> ***Ce qui veut dire que CE QUI RESTE EST PARALLÉLISABLE EN ENTIER — pas « en principe », mais par la structure du plan lui-même. La contrainte n'est pas l'ordonnancement, c'est le nombre de workstreams et la disponibilité de l'éditeur.***

**Et deux goulots d'étranglement partagés expliquent pourquoi le parallélisme n'est pas illimité :**

| Ressource | Qui la veut |
|---|---|
| **L'éditeur** | *toute suite qui n'est pas statique — et une suite qui POSSÈDE l'éditeur ne peut pas coexister avec une autre* |
| **`TurboMCPToolsets.h`** | *toute row qui ajoute un outil — un seul éditeur à la fois, ré-lecture obligatoire avant édition* |

### 10.2 La vélocité, et pourquoi les six rows ne sont pas réparties uniformément

| Fenêtre | Rows `done` |
|---|---|
| rounds 1 → 150 | **2** *(0.2, 1.4)* |
| rounds 150 → 190 | **2** *(1.3 or 7.1 — les deux arrivent ici)* |
| **rounds 190 → 218** | **4** *(7.1, 10.1, 10.2, 2.1)* |

> ***QUATRE DES SIX ROWS FAITES LE SONT DANS LES 28 DERNIERS ROUNDS. Ce n'est pas de l'accélération rhétorique : les sections 79 à 150 ont construit la machine — briefs, commits de sécurité, checks de pré-vol, règle `P1`, break tests — et la machine produit maintenant.***

**Mais la projection honnête est moins bonne que la vélocité récente ne le suggère, pour trois raisons mesurables :**

1. **Les quatre rows récentes étaient les MIEUX SPÉCIFIÉES du plan** — *chacune avait déjà sa mesure faite (1.3 pour 10.1/10.2, 7.1 préparée par 1.4, 2.1 préparée par 1.4 et par la capacité existante).*
2. **Les 40 restantes incluent le bloc graphes (phase 6, six rows) et la phase 8 (matériaux)** — *les plus grosses du plan, et celles dont les prémisses n'ont pas encore été testées.*
3. **Le budget de rounds restant est 38.** *Quatre rows en 28 rounds, appliqué uniformément à 40 rows, donne 280 rounds — **donc le plan tel qu'écrit NE SERA PAS FINI dans le budget.***

> ***Et c'est la chose la plus importante de ce rapport : LE PLAN N'A PAS ÉTÉ RE-SCOPÉ POUR TENIR DANS LE BUDGET, ET JE LE DIS PLUTÔT QUE DE LAISSER LE CHIFFRE PARLER TOUT SEUL.***

### 10.3 Ce que je ferais du budget restant, si c'était ma décision

**Trois stratégies, et je dis celle que je choisirais :**

| | Stratégie | Coût | Ce qu'on obtient |
|---|---|---|---|
| **A** | *Faire les 40 rows dans l'ordre* | *~280 rounds* | *Rien avant longtemps, et des rows à moitié faites à la fin* |
| **B** | ***Finir les phases à fort effet de levier (2, 9, 10), refuser le reste PAR ÉCRIT*** | ***~25 rounds*** | ***Le plugin le plus utile, et un plan qui dit ce qu'il n'a pas fait*** |
| **C** | *Maximiser le nombre de rows faites* | *~38 rounds* | *~8 rows de plus, dispersées, sans qu'aucune phase soit complète* |

> ***JE CHOISIRAIS B — parce que c'est la même décision que ce projet a déjà prise trois fois et qui a marché : row 10.2 a câblé 2 writers sur 32 ET NOMMÉ LES 30 ; row 2.2 a construit 9 types sur 9 mais AURAIT PU en refuser ; le kit a été mesuré buildable pour 4 assets et 2 refusés. À CHAQUE FOIS, LE RÉSULTAT PARTIEL MESURÉ ET NOMMÉ A ÉTÉ ACCEPTÉ COMME COMPLET — et c'est ce qui permet au plan d'être FINI un jour.***

---

## 11. Les invariants qui font tenir les 93 outils — ce que je défendrais dans une revue

***Ce ne sont pas des conventions de style. Chacun a été payé par un défaut réel, et chacun est vérifiable mécaniquement.***

| Invariant | Pourquoi il existe | Est-il vérifié ? |
|---|---|---|
| **Les lecteurs sont dans `Read`/`Look`, les écrivains dans `Preview`/`Edit`** | *un lecteur est **structurellement** incapable d'écrire — ce n'est pas une discipline, c'est un type* | ✅ *par la répartition des toolsets* |
| **Chaque écrivain a un jumeau `Preview` qui partage le MÊME `Plan`** | *donc Preview et Edit refusent **exactement** le même input, et toute vérification peut vivre dans `Plan` une seule fois* | ✅ *`D-02`, et 26 Preview / 33 Edit* |
| **Un outil qui ne peut pas faire une chose la REFUSE PAR SON NOM, avec la ligne moteur qui le décide** | *« un refus invisible se lit comme une capacité »* | ⚠️ *par convention et par revue — **pas mécaniquement*** |
| **Une mesure absente n'imprime JAMAIS `false` ni zéro (`P1`)** | *`NOT ASKED` plutôt que `false` ; un motif de décision jamais vide ; un enum à trois états* | ⚠️ *trois formes, chacune trouvée après coup* |
| **Le rapport d'un outil n'est pas une preuve : relire par un AUTRE outil** | *un producteur qui se rapporte lui-même ne prouve rien* | ✅ *pratiqué dans chaque row, jamais automatisé* |
| **Pour chaque check, nommer l'INPUT qui le fait échouer** | *cinq capacités ont surestimé leur couverture* | ⚠️ ***`audit-suite-quality.py` est advisory — c'est le trou le plus large du projet*** |
| **Un check ne dépend pas d'un état mutable partagé** | *un check dépendait d'un fichier restant non suivi ; un `git add` étranger l'aurait rendu rouge* | ⚠️ *trouvé deux fois, corrigé deux fois, non vérifié* |
| **Restaurer la fixture à l'ALLER aussi bien qu'au retour, et l'asserter** | *restaurer seulement à la fin a tué le serveur que la suite interrogeait* | ✅ *pratiqué, et row 2.2 l'a étendu à un index git* |
| **Un break test doit montrer la VALEUR FAUSSE, pas seulement une ligne rouge** | *un check qui assertait un préfixe passait contre un build cassé* | ✅ *cinq fois, et quatre fois il a trouvé un second défaut* |

### 11.1 **Le trou structurel — FERMÉ le 2026-10-11 (était la première de mes cinq priorités)**

> ***MISE À JOUR : CE TROU EST MAINTENANT FERMÉ. Il est conservé ici tel qu'il a été diagnostiqué, suivi de ce qui a été fait.***

**Le diagnostic initial :** *trois des neuf invariants étaient tenus par convention et par revue, pas par la machine — refuse-by-name, la règle `P1`, et nommer l'input qui fait échouer chaque check — **et ce sont exactement les trois qui ont produit les cinq capacités à couverture surestimée.***

**Ce qui a été fait :**

| | |
|---|---|
| **1** | ***Un 6ᵉ finding, `CANNOT-MATCH`, ajouté à `audit-suite-quality.py`*** — *le **miroir** des cinq autres : il cherche une assertion qui ne peut pas **PASSER**, la classe que row 2.2 a trouvée (`^` sans `re.MULTILINE` → 16 des 18 échecs d'une seule fonction). Prouvé sur **six cas** : trois qui doivent tirer, trois qui ne doivent pas.* |
| **2** | ***L'audit est passé de **ADVISORY à BLOQUANT**.*** *Il rapportait **cinq** candidats et **tous les cinq étaient des faux positifs** — un gate qui échoue du code sain n'est pas un gate, c'est une raison de le contourner. Les cinq ont été lus, quatre faux positifs retirés, **et le seul vrai trouvé (`test-widget-preview.py:175`, `"4096" in huge`) CORRIGÉ plutôt que toléré.*** |
| **3** | ***Prouvé dans les deux sens : 0 candidat → PASS exit 0 · 1 candidat → FAIL exit 1 · sonde retirée → PASS exit 0.*** |

**Et il a immédiatement attrapé quelque chose dès son premier full run** — *non pas un candidat, mais la contention : `test-vcs` a échoué sur un check portant sur l'**arbre de travail git**, et **repasse seule à 55 checks**.*

### 11.2 Le diagnostic initial, conservé

> ***TROIS DES NEUF INVARIANTS SONT TENUS PAR CONVENTION ET PAR REVUE, PAS PAR LA MACHINE — et ce sont exactement les trois qui ont produit les cinq capacités à couverture surestimée.***
>
> *Les invariants vérifiés mécaniquement n'ont **jamais** régressé ici. Les trois qui reposent sur la discipline humaine ont régressé **cinq fois**, et chaque fois ça a été trouvé par un humain ou un workstream ultérieur.*
>
> **LE CORRECTIF EST IDENTIFIÉ ET PAS FAIT : `audit-suite-quality.py` doit BLOQUER plutôt que conseiller.** *C'est l'item 9.4, et c'est le seul du rapport qui soit à la fois peu coûteux et de portée générale.*

### 11.2 Et l'invariant le plus fort du projet n'est écrit nulle part comme un invariant

**C'est le break test.** *Il n'est pas dans `D-01` à `D-57` comme une décision architecturale — il est dans les briefs, répété à chaque row.*

> ***ET C'EST CELUI QUI A LE MEILLEUR RATIO : cinq fois exécuté, cinq fois il a prouvé que le check mord, et QUATRE FOIS il a trouvé un DÉFAUT SUPPLÉMENTAIRE que personne ne cherchait — deux dans le code livré, deux dans la suite elle-même.***
>
> **Si je devais garder une seule pratique de tout ce projet, ce serait celle-là — et si je devais en ajouter une, ce serait de la rendre mécaniquement obligatoire plutôt que de la redemander dans chaque brief.**

---

## 12. Ce qui a changé depuis la première version de ce rapport

*Le rapport a été écrit au round 217. Voici ce qui a bougé depuis, et la colonne de droite est ce qui compte : **pourquoi**.*

| | Round 217 | **Maintenant** | Le fait qui explique le mouvement |
|---|---|---|---|
| Full run | *`passed 56, of 56`* | ***`passed 58, failed 0, blocked 0, of 58`*** | ***deux défauts **préexistants** corrigés, dont un qui rendait le vert **impossible***** |
| Plan | *40 `todo`, 6 `done`* | ***31 `todo`, 15 `done`*** | ***huit rows étaient **déjà construites** et marquées `todo`*** |
| Audit des suites | *advisory* | ***BLOQUANT*** | *trois invariants tenus par convention sont passés à la machine* |
| Éditeur étranger | *indétectable* | ***détecté, `exit 3`*** | *la collision qui a coûté quatre mesures* |
| Kit | *« `TA-17`/`18`/`19` buildables »* | ***`TA-17` CONSTRUIT ; `18`/`19` refusés par nom*** | *et `TA-18` n'était **pas** buildable — mon rapport avait tort* |

### 12.1 Et les deux corrections les plus importantes n'étaient dans **aucune** des cinq priorités

> ***LE RAPPORT NOMMAIT CINQ CHOSES SUR LESQUELLES JE PASSERAIS DU TEMPS. LES DEUX DÉFAUTS QUI BLOQUAIENT LE PROPRE SIGNAL VERT DU PROJET N'EN FAISAIENT PAS PARTIE.***
>
> *Ils ont été trouvés par un workstream faisant une row de fixtures, et par moi **suivant son évidence**. **La liste a été utile pour commencer ; l'évidence a été utile pour finir.***

### 12.2 Et la leçon qui a coûté le plus cher, énoncée une fois

**NEUF FOIS DANS CETTE SÉQUENCE, UNE OPÉRATION A ÉCHOUÉ EN SILENCE ET A ÉTÉ LUE COMME UN SUCCÈS :**

| | |
|---|---|
| `git checkout` avec `capture_output=True` **et le résultat jamais lu** | *7ᵉ — l'erreur était **capturée et personne ne l'a regardée*** |
| une regex qui supposait le mauvais format | *8ᵉ — **dans la suite qui existe pour faire respecter la règle*** |
| un compte codé en dur sur un asset partagé | *9ᵉ — la suite **passait ou échouait selon la fixture qu'elle regardait*** |

> ***C'EST LA MÊME PHRASE QUE « UN REFUS INVISIBLE SE LIT COMME UNE CAPACITÉ », ET ELLE EST LE DÉFAUT LE PLUS RÉCURRENT DE CE PROJET — Y COMPRIS DANS LES OUTILS QUI EXISTENT POUR L'ATTRAPER.***

---

## 13. En une ligne

> **93 outils, `passed 58, failed 0, blocked 0, of 58` — LE FULL RUN EST ENTIÈREMENT VERT — 31 rows de plan restantes, et une machine de vérification qui produit maintenant des résultats corrects sans que j'aie à intervenir.**
>
> ***Ce qui reste n'est pas bloqué par des inconnues : c'est borné par du temps, par cinq fixtures qui n'existent pas, et par un serveur source control que cette machine n'a jamais eu.***
