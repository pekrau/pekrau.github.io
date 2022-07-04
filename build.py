"Build the website by converting MD to HTML and creating index pages."

__version__ = "0.4.0"

import csv
import json
import os
import os.path
import re
import shutil
import time

import jinja2
import marko
import yaml


FRONT_MATTER_RX = re.compile(r"^---(.*?)---", re.DOTALL | re.MULTILINE)

POSTS = []                # List of posts sorted by date.
PAGES = []                # List of pages.
TAGS = {}                 # key: tag name; value: dict(name, value, posts)
CATEGORIES = {}           # key: category name; value: dict(name, value, posts)
BOOKS = {}                # key: "{lastname} {published}", optional resolving suffix
AUTHORS = {}              # key: name; value: canonical name
HTML_FILES = set()        # All HTML files created during a run.


# The Jinja2 template processing environment.
def author_link(author):
    return f"""<a href="/library/authors/{author}.html">{author}</a>"""

def authors_links(book):
    return "; ".join([author_link(a) for a in book["authors"]])

def book_link(book):
    return f"""<a href="/library/{book['isbn']}.html">{book['title']}</a>"""

env = jinja2.Environment(
    loader=jinja2.FileSystemLoader("templates"),
    autoescape=jinja2.select_autoescape(['html'])
)
env.globals["author_link"] = author_link
env.globals["authors_links"] = authors_links
env.globals["book_link"] = book_link
env.globals["len"] = len
env.globals["sorted"] = sorted


class HTMLRenderer(marko.html_renderer.HTMLRenderer):
    "Modify output for Bootstrap and other changes."

    def render_quote(self, element):
        "Add blockquote output class for Bootstrap."
        return '<blockquote class="blockquote ms-3">\n{}</blockquote>\n'.format(
            self.render_children(element)
        )

# Markdown converter.
MARKDOWN = marko.Markdown(renderer=HTMLRenderer)

def read_posts():
    "Read all Markdown files for blog posts and pre-process."
    for filename in os.listdir("source/posts"):
        if filename.endswith("~"): continue
        POSTS.append(read_md(os.path.join("source/posts", filename)))
    POSTS.sort(key=lambda p: p["date"], reverse=True)
    POSTS[0]["prev"] = POSTS[1]
    POSTS[-1]["next"] = POSTS[-2]
    for i, post in enumerate(POSTS[1:-1], start=1):
        post["next"] = POSTS[i-1]
        post["prev"] = POSTS[i+1]
    for post in POSTS:
        for tag in post.get("tags", []):
            try:
                TAGS[tag["name"]]["posts"].append(post)
            except KeyError:
                tag["posts"] = [post]
                TAGS[tag["name"]] = tag
    for tag in TAGS.values():
        tag["posts"].sort(key=lambda p: p["date"], reverse=True)
    for post in POSTS:
        for category in post.get("categories", []):
            try:
                CATEGORIES[category["name"]]["posts"].append(post)
            except KeyError:
                category["posts"] = [post]
                CATEGORIES[category["name"]] = category
    for category in CATEGORIES.values():
        category["posts"].sort(key=lambda p: p["date"], reverse=True)

def read_pages():
    """Read all Markdown files for pages and pre-process.
    Add ancient hard-coded HTML page trees and links to other subsites."""
    for filename in os.listdir("source/pages"):
        if filename.endswith("~"): continue
        PAGES.append(read_md(os.path.join("source/pages", filename)))
    PAGES.append({"name": "python",
                  "path": "/python/",
                  "title": "Python code",
                  "predefined": True})
    PAGES.append({"name": "lectures",
                  "path": "/lectures/",
                  "title": "Old lectures",
                  "predefined": True})
    PAGES.append({"name": "molscript",
                  "path": "/MolScript/",
                  "title": "MolScript",
                  "predefined": True,
                  "menu": True})
    PAGES.sort(key=lambda p: (p.get("level", 0), p["title"].lower()))

def read_books():
    "Read the Goodreads dump CSV file and apply any corrections."
    # Read lookup table of canonical author names.
    with open("source/authors_canonical.csv") as infile:
        reader = csv.DictReader(infile)
        for row in reader:
            AUTHORS[row["name"]] = row["canonical"]

    non_subjects = set(["currently-reading", "to-read", "read"])
    with open("source/goodreads_library_export.csv") as infile:
        reader = csv.DictReader(infile)
        for row in reader:
            book = {
                "title": row["Title"],
                "goodreads": row["Book Id"],
                "isbn": row["ISBN13"].lstrip("=").strip('"') or row["ISBN"].lstrip("=").strip('"'),
                "published": row["Original Publication Year"] or row["Year Published"],
                "edition": {"published": row["Year Published"],
                            "publisher": row["Publisher"]}
            }
            book["authors"] = [row["Author l-f"]]
            lastname = book["authors"][0].split(",")[0].strip()
            book["key"] = f"{lastname} {book['published']}"
            for name in row["Additional Authors"].split(","):
                name = name.strip()
                if not name: continue
                parts = name.split()
                lastname = parts[-1]
                firstname = " ".join(parts[0:-1])
                book["authors"].append(f"{lastname}, {firstname}")
            book["subjects"] = []
            subjects = [t.strip() for t in row["Bookshelves"].strip('"').split(",")]
            for subject in set(subjects).difference(non_subjects):
                book["subjects"].append(subject.replace("-", " ").capitalize())
            if row["My Rating"] and row["My Rating"] != "0":
                book["rating"] = int(row["My Rating"])
            if row["My Review"]:
                book["html"] = MARKDOWN.convert(row["My Review"].strip('"').replace("<br/>", "\n"))

            # Read any corrections file for the book.
            try:
                with open(f"source/corrections/{book['goodreads']}.json") as infile:
                    book.update(json.load(infile))
            except IOError:
                pass
            if book["key"] in BOOKS:
                raise ValueError(f"more than one book for {key}")
            BOOKS[book["key"]] = book

            # Normalize author names; do after corrections!
            for pos, author in enumerate(book["authors"]):
                author = author.replace(".", "").strip().rstrip(",")
                book["authors"][pos] =  AUTHORS.get(author, author)

    # Check the validity of book data.
    isbns = set()
    keys = dict()
    for book in BOOKS.values():
        if not book.get("published"):
            print(">>> lacking published:", book["goodreads"], book["title"])
        if not book["isbn"]:
            print(">>> lacking ISBN:", book["goodreads"], book["title"])
        if book["isbn"] in isbns:
            print(">>> duplicate isbn:", book["goodreads"], book["title"])
        elif book["isbn"]:
            isbns.add(book["isbn"])
        key = book["key"]
        if key in keys:
            print(">>> duplicate key:", book["goodreads"], book["title"])
            print("                  ", keys[key]["goodreads"], keys[key]["title"])
        else:
            keys[key] = book

def build_index():
    "Build the top index.html file."
    categories = list(CATEGORIES.values())
    categories.sort(key=lambda c: c["value"].lower())
    # Convert the two first paragraphs of the first three posts.
    posts = POSTS[:3]
    for post in posts:
        post["short_html"] = MARKDOWN.convert("\n\n".join(post["content"].split("\n\n")[:2]))
    build_html("index.html",
               updated=time.strftime("%Y-%m-%d"),
               categories=categories,
               posts=posts)

def build_blog():
    "Build blog post files, index.html and list.html files for the blog."
    build_html("blog/index.html", "blog/index.html", posts=POSTS)
    en_posts = [p for p in POSTS if p.get("language") == "en"]
    build_html("blog/en/index.html", "blog/en/index.html", posts=en_posts)
    for post in POSTS:
        build_html(os.path.join(post["path"].strip("/"), "index.html"),
                   template="blog/post.html", 
                   post=post,
                   language=post.get("language", "sv"))
    tags = sorted(TAGS.values(), key=lambda t: t["value"].lower())
    build_html("blog/tags/index.html", tags=tags)
    for tag in tags:
        build_html(os.path.join("blog/tags", tag["name"], "index.html"),
                   template="blog/tags/tag.html",
                   tag=tag,
                   posts=tag["posts"])
    categories = sorted(CATEGORIES.values(), key=lambda c: c["value"].lower())
    build_html("blog/categories/index.html", categories=categories)

def build_pages():
    "Build page files."
    for page in PAGES:
        if page.get("predefined"): continue
        build_html(os.path.join(page["path"].strip("/"), "index.html"),
                   template="page.html", 
                   page=page,
                   language=page.get("language", "sv"))

def build_books():
    "Build book files."
    books = list(sorted([b for b in BOOKS.values() if b.get("isbn")], 
                        key=lambda b: b["key"]))
    build_html("library/index.html", books=books)
    for book in books:
        build_html(f"library/{book['isbn']}.html",
                   template="library/book.html",
                   book=book)
    authors = {}
    for book in books:
        for author in book["authors"]:
            authors.setdefault(author, []).append(book)
    build_html("library/authors/index.html", authors=authors)
    for author, books in authors.items():
        build_html(f"library/authors/{author}.html",
                   template="library/authors/author.html",
                   author=author,
                   books=books)

def build_html(filepath, template=None, pages=None, **kwargs):
    "Build a single HTML page from the data for an item."
    if template is None:
        template = filepath
    if pages is None:
        pages = PAGES
    template = env.get_template(template)
    filepath = os.path.join("docs", filepath)
    dirpath = os.path.dirname(filepath)
    if dirpath and not os.path.exists(dirpath):
        os.makedirs(dirpath)
    with open(filepath, "w") as outfile:
        outfile.write(template.render(pages=pages, **kwargs))
    HTML_FILES.add(filepath)

def read_md(filepath):
    "Return the Markdown file as a dict with front matter and content as items."
    with open(filepath) as infile:
        data = infile.read()
    match = FRONT_MATTER_RX.match(data)
    if match:
        try:
            result = yaml.safe_load(match.group(1)) or {}
        except yaml.scanner.ScannerError as error:
            raise IOError(f"YAML problem in {filepath}: {error}")
        except yaml.composer.ComposerError as error:
            raise IOError(f"YAML problem in {filepath}: {error}")
        except yaml.parser.ParserError as error:
            raise IOError(f"Invalid YAML in {filepath}: {error}")
        result["content"] = data[match.end():]
    else:
        result = {"content": data}
    result["html"] = MARKDOWN.convert(result["content"])
    result.get("tags", []).sort(key=lambda c: c["value"].lower())
    result.get("categories", []).sort(key=lambda c: c["value"].lower())
    return result

def cleanup_html_files():
    "Remove any HTML files not created during this run."
    CURRENT_FILES = set()
    for dirpath, dirnames, filenames in os.walk("docs"):
        if dirpath.startswith("docs/files"): continue
        if dirpath.startswith("docs/python"): continue
        if dirpath.startswith("docs/lectures"): continue
        for filename in filenames:
            if not filename.endswith("html"): continue
            CURRENT_FILES.add(os.path.join(os.path.normpath(dirpath), filename))
    for filepath in CURRENT_FILES.difference(HTML_FILES):
        os.remove(filepath)
        print("deleted", filepath)
    # Remove directories that have become empty.
    changed = True
    while changed:
        changed = False
        for dirpath, dirnames, filenames in os.walk("docs"):
            if not (dirnames or filenames):
                shutil.rmtree(dirpath)
                print("deleted", dirpath)
                changed = True

if __name__ == "__main__":
    read_posts()
    read_pages()
    read_books()
    build_index()
    build_blog()
    build_pages()
    build_books()
    cleanup_html_files()
