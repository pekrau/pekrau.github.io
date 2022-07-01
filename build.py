"Build the website by converting MD to HTML and creating index pages."

__version__ = "0.3.0"


import os
import os.path
import re
import shutil
import time

import jinja2
import marko
import yaml


FRONT_MATTER_RX = re.compile(r"^---(.*?)---", re.DOTALL | re.MULTILINE)

POSTS = []                # sorted by date
PAGES = []
TAGS = {}                 # key: tag name; value: dict(name, value, posts)
CATEGORIES = {}           # key: category name; value: dict(name, value, posts)
BOOKS = []
BOOKS_LOOKUP = {}         # key: "{lastname} {published}", optional resolving suffix
HTML_FILES = set()        # All files created during a run.


# The Jinja2 template processing environment.
env = jinja2.Environment(
    loader=jinja2.FileSystemLoader("templates"),
    autoescape=jinja2.select_autoescape(['html'])
)
env.globals["len"] = len


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
    "Read all Markdown files for books."
    for filename in os.listdir("source/books"):
        if filename.endswith("~"): continue
        BOOKS.append(read_md(os.path.join("source/books", filename)))
    # Set the key for the book, if not already set.
    for book in BOOKS:
        if "key" not in book:
            lastname = book["authors"][0].split(",")[0].strip()
            book["key"] = f"{lastname} {book['published']}"
        BOOKS_LOOKUP.setdefault(book["key"], []).append(book)
    for key, books in BOOKS_LOOKUP.items():
        if len(books) > 1:
            print(key)
            for book in books:
                print("  ", book["goodreads"], book["title"])

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
                   template_filepath="blog/post.html", 
                   post=post,
                   language=post.get("language", "sv"))
    tags = sorted(TAGS.values(), key=lambda t: t["value"].lower())
    build_html("blog/tags/index.html", tags=tags)
    for tag in tags:
        build_html(os.path.join("blog/tags", tag["name"], "index.html"),
                   template_filepath="blog/tags/tag.html",
                   tag=tag,
                   posts=tag["posts"])
    categories = sorted(CATEGORIES.values(), key=lambda c: c["value"].lower())
    build_html("blog/categories/index.html", categories=categories)

def build_pages():
    "Build page files."
    for page in PAGES:
        if page.get("predefined"): continue
        build_html(os.path.join(page["path"].strip("/"), "index.html"),
                   template_filepath="page.html", 
                   page=page,
                   language=page.get("language", "sv"))

def build_books():
    "Build book files."
    books = list(sorted(BOOKS, key=lambda b: (b["authors"], b["published"])))
    build_html("library/index.html", books=books)
    for book in BOOKS:
        if not book.get("isbn"): continue
        build_html(f"library/{book['isbn']}.html",
                   template_filepath="library/book.html",
                   book=book)

def build_html(html_filepath, template_filepath=None, pages=None, **kwargs):
    "Build a single HTML page from the data for an item."
    if template_filepath is None:
        template_filepath = html_filepath
    if pages is None:
        pages = PAGES
    template = env.get_template(template_filepath)
    html_filepath = os.path.join("docs", html_filepath)
    dirpath = os.path.dirname(html_filepath)
    if dirpath and not os.path.exists(dirpath):
        os.makedirs(dirpath)
    with open(html_filepath, "w") as outfile:
        outfile.write(template.render(pages=pages, **kwargs))
    HTML_FILES.add(html_filepath)

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
