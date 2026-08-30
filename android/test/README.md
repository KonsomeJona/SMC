# Harnais de test Android

## Le niveau de test

`smc/data/levels/test_all.smclvl` rassemble ce qu'un monde normal ne permet
pas d'exercer en une passe :

- **fleur de feu, champignon, etoile, lune posés au sol** — le bouton tir ne
  peut rien faire tant que le joueur est en petit Maryo, et frapper une boite
  en sautant pile dessous n'est pas reproductible en test ;
- **un de chaque ennemi, marchant dans les deux sens** — furball, turtle,
  krush, gee, spikeball, plus eato et flyon qui sont ancrés. C'est le test du
  retournement des sprites ;
- **une boite a texte** — le dialogue qui met le jeu en pause ;
- **un mur de trois blocs** et **un trou de 150 px** — le saut tenu et le saut
  franchi ;
- **les deux sortes de sortie** : faisceau (HAUT quand on est dessus) a
  x=3600, et warp vers le bas dans un tuyau a x=4200.

### Y entrer

Le niveau n'est atteignable que par un fichier, donc un joueur ne le voit
jamais :

```bash
adb shell run-as me.takohi.bandagoo touch files/testlevel
```

Au lancement suivant le jeu démarre directement dedans (`SMCTEST booting into
test_all` dans logcat). Retirer le fichier rend le démarrage normal.

### Piège : les assets ne se réextraient pas

`SMCActivity` n'extrait `assets/data/` que lorsque le **versionCode** change.
C'est correct en production, mais en développement le versionCode ne bouge
pas : un niveau modifié ne parvient jamais sur l'appareil. Avant de tester une
modification de données :

```bash
adb shell pm clear me.takohi.bandagoo   # efface aussi le flag
```

puis relancer une fois l'app et recréer `files/testlevel`.

## Les autres scripts

| Script | Rôle |
|---|---|
| `instrumental-test.sh` | 23 vérifications, du démarrage au cycle de vie |
| `enter-level.sh` | depuis un démarrage à froid, entre dans le niveau 1 |
| `watch-run.sh` | supervise un run : ferme les dialogues, relance après un game over, rapporte `LEVEL_FINISHED` |
| `plan-level.py` | extrait ennemis, trous, murs et sortie d'un `.smclvl` |

L'autopilot interne s'active par un fichier `autoplay` dans le même
répertoire ; un fichier `nogod` a côté lui retire l'invincibilité.
