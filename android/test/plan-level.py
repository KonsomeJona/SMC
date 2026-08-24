#!/usr/bin/env python3
"""plan-level.py — derive a jump plan from a level file.

A purely reactive pilot dies here: small Maryo is killed by any contact, and a
stall is only measurable once the enemy has already arrived. The level XML
holds everything needed to know the hazards in advance:

  - enemies      jump before reaching them
  - real gaps    holes in the ground, jump before the edge
  - walls        a step up of more than one crate, which running cannot clear

Walls matter most: the run stalled for hours against a five-crate stack whose
only way up is a single crate used as a step, and no amount of jump-when-stuck
aims at it.

Usage: plan-level.py LEVEL.smclvl
Prints "exit=<x> ..." then a comma separated list of jump positions.
"""
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

TILE = 43  # crate size in this tileset


def analyse(path):
    root = ET.parse(path).getroot()
    props = lambda el: {p.get('name'): p.get('value') for p in el}

    massives, enemies, exits = [], [], []
    for el in root:
        d = props(el)
        if el.tag == 'sprite' and d.get('type') == 'massive':
            massives.append((float(d.get('posx', 0)), float(d.get('posy', 0))))
        elif el.tag == 'enemy':
            enemies.append(float(d.get('posx', 0)))
        elif el.tag == 'levelexit':
            exits.append(float(d.get('posx', 0)))

    # Ground line and holes in it.
    ymax = max(y for _, y in massives)
    ground = sorted(x for x, y in massives if y >= ymax - 40)
    gaps = [(a, b) for a, b in zip(ground, ground[1:]) if b - a > 100]

    # Height profile: highest massive surface per column (posy grows downward).
    col = defaultdict(list)
    for x, y in massives:
        col[int(x // TILE)].append(y)
    prof = {c: min(ys) for c, ys in sorted(col.items())}

    walls, prev = [], None
    for c in sorted(prof):
        top = prof[c]
        if prev is not None and prev - top > 60:
            walls.append(c * TILE)
        prev = top

    return massives, enemies, gaps, walls, (min(exits) if exits else None)


def plan(path):
    _, enemies, gaps, walls, exit_x = analyse(path)
    jumps = set()
    jumps |= {round(x - 110) for x in enemies}   # clear the enemy
    jumps |= {round(a - 60) for a, _ in gaps}    # leave before the edge
    for w in walls:                              # approach, then climb
        jumps |= {round(w - 95), round(w - 35)}
    return sorted(j for j in jumps if j > 0), exit_x, enemies, gaps, walls


if __name__ == '__main__':
    jumps, exit_x, enemies, gaps, walls = plan(sys.argv[1])
    print("exit=%s enemies=%d gaps=%d walls=%d" %
          (exit_x, len(enemies), len(gaps), len(walls)))
    print(','.join(str(j) for j in jumps))
