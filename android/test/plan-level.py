#!/usr/bin/env python3
"""plan-level.py — derive a jump plan from a level file.

A reactive pilot dies in this game: small Maryo is killed by any contact, and
by the time a stall is measurable the enemy has already reached him. The level
XML holds every enemy position and every hole in the ground, so the hazards can
be known before the run starts.

Usage: plan-level.py smc/data/levels/lvl_1.smclvl
"""
import sys, xml.etree.ElementTree as ET


def plan(path):
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

    ymax = max(y for _, y in massives)
    ground = sorted(x for x, y in massives if y >= ymax - 40)
    gaps = [(a, b) for a, b in zip(ground, ground[1:]) if b - a > 100]

    jumps = sorted({round(x - 110) for x in enemies} | {round(a - 60) for a, _ in gaps})
    return [j for j in jumps if j > 0], (min(exits) if exits else None), enemies, gaps


if __name__ == '__main__':
    jumps, exit_x, enemies, gaps = plan(sys.argv[1])
    print("exit=%s enemies=%d gaps=%d" % (exit_x, len(enemies), len(gaps)))
    print(','.join(str(j) for j in jumps))
