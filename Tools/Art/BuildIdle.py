"""Entrada compatible para recuperar el reposo aprobado sin regenerar dibujos ni insignias."""
from __future__ import annotations

import argparse
from pathlib import Path

import BuildUnits as U


def patch_idle(root,preview_only=False):
    return U.restore_approved_idle(root,preview_only=preview_only)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root',type=Path,default=U.ROOT)
    parser.add_argument('--preview-only',action='store_true')
    args=parser.parse_args()
    patch_idle(args.project_root,args.preview_only)


if __name__=='__main__':main()
