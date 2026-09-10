"""Generate all four editable performances from the approved design directions."""

import argparse

from concept_ink_work import ink
from concept_corridor_work import corridor
from concept_spatial_works import dunhuang_ribbons, vortex, porcelain

RECIPES = {'aureate_vortex': vortex, 'porcelain_bloom': porcelain,
           'stratified_ink': ink, 'lumen_corridor': corridor,
           'dunhuang_ribbons': dunhuang_ribbons}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', choices=RECIPES, action='append')
    args = parser.parse_args()
    for name in args.name or RECIPES:
        RECIPES[name]()


if __name__ == '__main__':
    main()
