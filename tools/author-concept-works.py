"""Generate all four editable performances from the approved design directions."""

import argparse

from concept_organic_works import vortex, ink
from concept_stage_works import porcelain, corridor

RECIPES = {'aureate_vortex': vortex, 'porcelain_bloom': porcelain,
           'stratified_ink': ink, 'lumen_corridor': corridor}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', choices=RECIPES, action='append')
    args = parser.parse_args()
    for name in args.name or RECIPES:
        RECIPES[name]()


if __name__ == '__main__':
    main()
