"Convert the BibTex entry in the clipboard to a reference YAML file."

from pathlib import Path
import string
import sys
import unicodedata

import bibtexparser
import pyperclip
import yaml

import latex_utf8


LIBRARY_SOURCE_DIR = Path("/home/pekrau/Dropbox/pekrau.github.io/source/others")
LIBRARY_TARGET_DIR = Path("/home/pekrau/Dropbox/pekrau.github.io/docs/library/references")

MONTHS = {
    "jan": 1,
    "january": 1,
    "feb": 2,
    "february": 2,
    "mar": 3,
    "march": 3,
    "apr": 4,
    "april": 4,
    "may": 5,
    "jun": 6,
    "june": 6,
    "jul": 7,
    "july": 7,
    "aug": 8,
    "august": 8,
    "sep": 9,
    "september": 9,
    "oct": 10,
    "october": 10,
    "nov": 11,
    "november": 11,
    "dec": 12,
    "december": 12,
}


def cleanup_latex(value):
    "Convert LaTeX characters to UTF-8, remove newlines and normalize blanks."
    return latex_utf8.from_latex_to_utf8(" ".join(value.split()))


def cleanup_whitespaces(value):
    "Replace all whitespaces with blanks."
    return " ".join(value.split())


def normalize(reference):
    """Normalize the reference for part of a URL.
    - Normalize non-ASCII characters.
    - Convert the string to ASCII.
    - Lowercase.
    - Replace blank with dash.
    """
    return unicodedata.normalize("NFKD", reference).\
        replace("æ", "a").\
        replace("Æ", "A").\
        encode("ASCII", "ignore").\
        decode("utf-8").\
        lower().\
        replace(" ", "-")


for entry in bibtexparser.loads(pyperclip.paste()).entries:
    data = dict(authors=cleanup_latex(entry.get("author", "")).split(" and "),
                type=entry.get("ENTRYTYPE") or constants.ARTICLE,
                year=int(entry["year"]))
    if editors := cleanup_latex(entry.get("editor", "")):
        data["editors"] = editors.split(" and ")
    for key, value in entry.items():
        if key in ("author", "editor", "ID", "ENTRYTYPE", "year"):
            continue
        data[key] = cleanup_latex(value).strip()
    # Do some post-processing.
    # Change month into date; sometimes has day number.
    month = cleanup_latex(data.pop("month", ""))
    parts = month.split("~")
    if len(parts) == 2 and parts[1]:
        month = MONTHS[parts[1].strip().casefold()]
        day = int("".join([c for c in parts[0] if c in string.digits]))
        data["published"] = f'{entry["year"]}-{month:02d}-{day:02d}'
    elif len(parts) == 1 and parts[0]:
        month = MONTHS[parts[0].strip().casefold()]
        data["published"] = f'{entry["year"]}-{month:02d}-00'
    # Split up keywords
    if keywords := data.pop("keywords", None):
        data["keywords"] = [cleanup_latex(k).strip() for k in keywords.split(";")]
    # Issue instead of number.
    try:
        data["issue"] = data.pop("number")
    except KeyError:
        pass
    # Change page numbers double dash to single dash.
    if pages := data.pop("pages", None):
        data["pages"] = pages.replace("--", "-")
    if abstract:= data.pop("abstract", None):
        data["abstract"] = cleanup_latex(cleanup_whitespaces(abstract))

    reference = f'{data["authors"][0].split(",")[0]} {data["year"]}'
    stem = normalize(reference)

    # Is there a set of references for the same year?
    if (filepath := LIBRARY_SOURCE_DIR / f"{stem}a.yaml").exists():
        for suffix in string.ascii_lowercase:
            if not (filepath := LIBRARY_SOURCE_DIR / f"{stem}{suffix}.yaml").exists():
                reference = reference + suffix
                break

    # Is there a previous reference? If so, rename by adding suffix 'a' to it.
    elif (filepath := LIBRARY_SOURCE_DIR / f"{stem}.yaml").exists():
        print(" ", filepath, "already exists")
        if input("overwrite it? > ") in "yYjJ":
            pass
        elif input("rename it? > ") in "yYjJ":
            changed_reference = reference + "a"
            changed_stem = stem + "a"
            for root in [LIBRARY_SOURCE_DIR, LIBRARY_TARGET_DIR]:
                source = root / f"{stem}.yaml"
                try:
                    with open(source) as infile:
                        changed_data = yaml.safe_load(infile)
                    changed_data["reference"] = changed_reference
                    target = root / f"{changed_stem}.yaml"
                    target.write_text(yaml.dump(changed_data, allow_unicode=True))
                    source.unlink()
                    print(f"renamed {source} to {target}")
                except FileNotFoundError:
                    pass
            reference = reference + "b"
        else:
            print("no action; reference not saved")
            sys.exit()

    data["reference"] = reference
    stem = normalize(reference)

    filepath = LIBRARY_SOURCE_DIR / f"{stem}.yaml"
    filepath.write_text(yaml.dump(data, allow_unicode=True))
    print("wrote", filepath)

    filepath = LIBRARY_TARGET_DIR / f"{stem}.yaml"
    filepath.write_text(yaml.dump(data, allow_unicode=True))
    print("wrote", filepath)
