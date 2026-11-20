"""Release archives and ISO images must never be written into the shared workspace."""
from pathlib import Path
import sys

PROJECT = Path(__file__).resolve().parents[1]
FORBIDDEN = (Path('/home/user').resolve(), PROJECT)


def external(path):
    out = Path(path).expanduser().resolve()
    if any(out == root or root in out.parents for root in FORBIDDEN):
        raise SystemExit('Destinazione rifiutata: ZIP e ISO vanno creati fuori dal workspace, ad esempio in /var/tmp.')
    return out


if __name__ == '__main__':
    for value in sys.argv[1:]:
        external(value)
