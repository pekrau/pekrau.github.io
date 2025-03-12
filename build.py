"Build the website by converting MD to HTML and creating index pages."

__version__ = "0.13.1"

import csv
import datetime
import email.utils
from hashlib import sha1
import json
import math
import os
import os.path
import re
import shutil
import time

import jinja2
import markupsafe
import marko
import yaml

BASE_URL = "https://pekrau.github.io"    # No trailing "/" slash!
GOODREADS_FILENAME = "source/goodreads_library_export.csv"
AUTHORS_CANONICAL_FILENAME = "source/authors_canonical.csv"

FRONT_MATTER_RX = re.compile(r"^---(.*?)---", re.DOTALL | re.MULTILINE)

POSTS = []                # List of posts sorted by date.
REDIRECTED_POSTS = []     # Deleted posts that redirect to another URL.
PAGES = []                # List of pages.
REDIRECTED_PAGES = []     # Deleted pages that redirect to another URL.
TAGS = {}                 # key: tag name; value: dict(name, value, posts)
CATEGORIES = {}           # key: category name; value: dict(name, value, posts)
BOOKS = {}                # key: "{lastname} {published}", optional resolving suffix
AUTHORS = {}              # key: name; value: canonical name
HTML_FILES = set()        # All HTML files created during a run.
SITEMAP_URLS = {};        # Key: canonical URL, value: lastmod.
MAX_LATEST_ITEMS = 10     # Max latest item on index page and in RSS 'feed.xml' file.
LATEST_ITEMS = []         # The latest items; posts and book reviews.
YEAR_PUBLISHED = {}       # key: year; value: list of books


# Setup the Jinja2 template processing environment.
def post_link(post):
    return markupsafe.Markup(f"""<a href="{post['path']}">{post['title']}</a>""")

def author_link(author, display=False):
    if display:
        name = author_display(author)
    else:
        name = author
    return markupsafe.Markup(f"""<a href="/library/authors/{author}">{name}</a>""")

def authors_links(book, max=3, display=False):
    if max and len(book["authors"]) > max:
        return markupsafe.Markup("; ".join([author_link(a, display=display)
                                            for a in book["authors"][:max]] + ["..."]))
    else:
        return markupsafe.Markup("; ".join([author_link(a, display=display)
                                            for a in book["authors"]]))

def author_display(author):
    "Display version of the author name: firstname lastname."
    try:
        lastname, firstname = author.split(",", 1)
    except ValueError:
        return author
    else:
        return f"{firstname.strip()} {lastname.strip()}"

def book_link(book, full=False):
    if full:
        return markupsafe.Markup(" ".join(["; ".join(book["authors"]),
                                           book_link(book),
                                           f"({book['published']})"]))
    else:
        return markupsafe.Markup(f"""<a href="/library/{book['isbn']}.html">{book['title']}</a>""")

def category_link(category):
    return markupsafe.Markup(f"""<a href="/blog/categories/{category['name']}" class="text-nowrap">{category['value']}</a>""")

def tag_link(tag, sized=True):
    try:
        number = len(tag["posts"])
    except KeyError:
        tag = TAGS[tag["name"]]
        number = len(tag["posts"])
    if sized:
        factor = 50 * (math.log(number) + 1.5)
        span = f"""<span style="font-size: {factor}%;">{tag['value']}</span>"""
    else:
        span = tag['value']
    href = f"/blog/tags/{tag['name']}"
    return markupsafe.Markup(f'<a href="{href}" class="text-nowrap" title="{number}">{span}</a>')

def subject_link(subject):
    title = " ".join([p.capitalize() for p in subject.split("-")])
    return markupsafe.Markup(f"""<a href="/library/subjects/{subject}" class="text-nowrap">{title}</a>""")

def published_link(year):
    return markupsafe.Markup(f"""<a href="/library/published/{year}" class="text-nowrap">{year}</a>""")


env = jinja2.Environment(
    loader=jinja2.FileSystemLoader("templates"),
    autoescape=jinja2.select_autoescape(['html'])
)
env.globals["post_link"] = post_link
env.globals["author_link"] = author_link
env.globals["authors_links"] = authors_links
env.globals["author_display"] = author_display
env.globals["book_link"] = book_link
env.globals["category_link"] = category_link
env.globals["tag_link"] = tag_link
env.globals["subject_link"] = subject_link
env.globals["published_link"] = published_link
env.globals["len"] = len
env.globals["sorted"] = sorted


class HTMLRenderer(marko.html_renderer.HTMLRenderer):
    "Modify output for Bootstrap 5."

    def render_quote(self, element):
        """Add blockquote output class for Bootstrap 5.
        If the last element is a source, then it is rendered as a blockquote footer.
        """
        source = None
        if len(element.children) >=2:
            source = self.render_children(element.children[-1])
            if source.startswith("source:"):
                source = source[len("source:"):].strip()
                element.children.pop()
            else:
                source = None
        text = self.render_children(element)
        if source:
            return f'''<figure class="ms-3">
<blockquote class="blockquote">{text}</blockquote>
<figcaption class="blockquote-footer">{source}</figcaption>
</figure>'''
        else:
            return f'''<figure class="ms-3">
<blockquote class="blockquote">{text}</blockquote>
</figure>'''

# Markdown converter.
MARKDOWN = marko.Markdown(renderer=HTMLRenderer)

def read_posts():
    "Read all Markdown files for blog posts and pre-process."
    for filename in sorted(os.listdir("source/posts")):
        if not filename.endswith(".md"): continue
        post = read_md(f"source/posts/{filename}")
        for key in ["name", "date", "categories"]:
            if key not in post:
                raise ValueError(f"post {filename} lacks '{key}'")
        post["path"] = f"/{post['date'].replace('-','/')}/{post['name']}/"
        if post.get("redirect"):
            REDIRECTED_POSTS.append(post)
        elif post.get("draft"):
            print("Draft >>>", filename)
        else:
            POSTS.append(post)
    POSTS.sort(key=lambda p: p["date"], reverse=True)
    POSTS[0]["prev"] = POSTS[1]
    POSTS[-1]["next"] = POSTS[-2]
    for i, post in enumerate(POSTS[1:-1], start=1):
        post["next"] = POSTS[i-1]
        post["prev"] = POSTS[i+1]
    # Remove the tag "bok".
    for post in POSTS:
        for pos, tag in enumerate(post.get("tags", [])):
            if tag["name"] == "bok":
                post["tags"].pop(pos)
                break
    # Set references to/from tags.
    for post in POSTS:
        for tag in post.get("tags", []):
            try:
                TAGS[tag["name"]]["posts"].append(post)
            except KeyError:
                tag["posts"] = [post]
                TAGS[tag["name"]] = tag
    for tag in TAGS.values():
        tag["posts"].sort(key=lambda p: p["date"], reverse=True)
    # Set references to/from categories.
    for post in POSTS:
        for category in post.get("categories", []):
            try:
                CATEGORIES[category["name"]]["posts"].append(post)
            except KeyError:
                category["posts"] = [post]
                CATEGORIES[category["name"]] = category
    for category in CATEGORIES.values():
        category["posts"].sort(key=lambda p: p["date"], reverse=True)
    # Convert any 'about' text'.
    for post in POSTS:
        if post.get("about"):
            post["about_html"] = MARKDOWN.convert(post["about"])

def read_pages():
    """Read all Markdown files for pages and pre-process.
    Add ancient hard-coded HTML page trees and links to other subsites."""
    for filename in sorted(os.listdir("source/pages")):
        if filename.endswith("~"): continue
        page = read_md(f"source/pages/{filename}")
        if page.get("redirect"):
            REDIRECTED_PAGES.append(page)
        else:
            PAGES.append(page)
    PAGES.sort(key=lambda p: (p.get("ordinal", 999), p["title"].lower()))

def read_books():
    "Read the Goodreads dump CSV file and apply any corrections."
    # Read lookup table of canonical author names.
    with open(AUTHORS_CANONICAL_FILENAME) as infile:
        reader = csv.DictReader(infile)
        for row in reader:
            AUTHORS[row["name"]] = row["canonical"]

    non_subjects = set(["currently-reading", "to-read", "read", "reviewed", "svenska"])
    with open(GOODREADS_FILENAME) as infile:
        reader = csv.DictReader(infile)
        for row in reader:
            book = {
                "type": "book",
                "title": row["Title"],
                "goodreads": row["Book Id"],
                "isbn": row["ISBN13"].lstrip("=").strip('"') or row["ISBN"].lstrip("=").strip('"'),
                "published": row["Original Publication Year"] or row["Year Published"],
                "edition": {"published": row["Year Published"] or row["Original Publication Year"],
                            "publisher": row["Publisher"]}
            }
            book["authors"] = [row["Author l-f"]]
            lastname = book["authors"][0].split(",")[0].strip()
            book["reference"] = f"{lastname} {book['published']}"
            for name in row["Additional Authors"].split(","):
                name = name.strip()
                if not name: continue
                parts = name.split()
                lastname = parts[-1]
                firstname = " ".join(parts[0:-1])
                book["authors"].append(f"{lastname}, {firstname}")
            subjects = [t.strip() for t in row["Bookshelves"].strip('"').split(",")]
            if "svenska" in subjects:
                book["language"] = "sv"
            else:
                book["language"] = "en"
            book["subjects"] = sorted(set(subjects).difference(non_subjects))
            if row["My Rating"] and row["My Rating"] != "0":
                book["rating"] = int(row["My Rating"])
            if row["My Review"]:
                book["content"] = row["My Review"].strip('"').replace("<br/>", "\n")
                book["html"] = MARKDOWN.convert(book["content"])
            book["lastmod"] = row["Date Added"].replace("/", "-")
            if row["Date Read"]:
                book["date"] = row["Date Read"].replace("/", "-")
                book["lastmod"] = book["date"]

            # Read any corrections file for the book.
            try:
                with open(f"source/corrections/{book['goodreads']}.json") as infile:
                    book.update(json.load(infile))
            except IOError:
                pass

            if book["reference"] in BOOKS:
                raise ValueError(f"duplicate reference for {book['goodreads']}, {book['title']} and {BOOKS[book['reference']]['goodreads']}, {BOOKS[book['reference']]['title']}")
            BOOKS[book["reference"]] = book
            try:
                YEAR_PUBLISHED.setdefault(int(book["published"]), list()).append(book)
            except ValueError:
                print(">>> invalid published:", book["goodreads"], book["title"])

            # Normalize author names; do after corrections!
            for pos, author in enumerate(book["authors"]):
                author = author.replace(".", "").strip().rstrip(",")
                book["authors"][pos] =  AUTHORS.get(author, author)

            # Set the path for the book file; do after corrections!
            book["path"] = f"/library/{book['isbn']}.html"

    # Collect and check ISBN data.
    isbns = set()
    for book in BOOKS.values():
        if not book["isbn"]:
            print(">>> lacking ISBN:", book["goodreads"], book["title"])
        if book["isbn"] in isbns:
            print(">>> duplicate isbn:", book["goodreads"], book["title"])
        elif book["isbn"]:
            isbns.add(book["isbn"])


def build_index():
    "Build the top index.html file."
    categories = list(CATEGORIES.values())
    categories.sort(key=lambda c: c["value"].lower())
    # Collect the latest blog posts.
    posts = POSTS[:MAX_LATEST_ITEMS]
    # Convert the first paragraph of the latest posts.
    for post in posts:
        try:
            content = post["content"][:post["content"].index("<!-- more -->")]
        except ValueError:
            content = post["content"].split("\n\n", 1)[0]
        post["short_html"] = MARKDOWN.convert(content)
    # Collect the latest book reviews.
    books = [b for b in BOOKS.values() if b.get("date") and b.get("content")]
    books.sort(key=lambda b: b["date"], reverse=True)
    books = books[:MAX_LATEST_ITEMS]
    for book in books:
        book["short_html"] = MARKDOWN.convert(book["content"].split("\n", 1)[0])
    items = posts + books
    items.sort(key=lambda b: b["date"], reverse=True)
    LATEST_ITEMS.extend(items[:MAX_LATEST_ITEMS])
    popular = [p for p in POSTS if p.get("popular")]
    build_html("index.html",
               sitemap=True,
               updated=time.strftime("%Y-%m-%d"),
               categories=categories,
               recent=LATEST_ITEMS,
               popular=popular)

def build_blog():
    "Build blog post files, index.html and list.html files for the blog."
    # Index of all blog posts.
    build_html("blog/index.html", posts=POSTS)
    # Index of all blog posts in English.
    en_posts = [p for p in POSTS if p.get("language") == "en"]
    build_html("blog/en/index.html", posts=en_posts)
    # All blog post pages.
    for post in POSTS:
        try:
            references = []
            for ref in post.get("references", []):
                references.append(BOOKS[ref])
        except KeyError:
            print(f"reference error '{ref}' in post '{post['title']}'")
            references = []
        build_html(f"{post['path'].strip('/')}/index.html",
                   template="blog/post.html", 
                   sitemap=True,
                   post=post,
                   language=post.get("language", "sv"),
                   description=", ".join([c["value"] for c in post.get("categories", [])]),
                   keywords=", ".join([t["value"] for t in post.get("tags", [])]),
                   author=post.get("author", "Per Kraulis"),
                   references=references,
                   lastmod=post["lastmod"])
    # Index of all tags.
    tags = sorted(TAGS.values(), key=lambda t: t["value"].lower())
    build_html("blog/tags/index.html", sitemap=True, tags=tags)
    for tag in tags:
        build_html(f"blog/tags/{tag['name']}/index.html",
                   template="blog/tags/tag.html",
                   tag=tag,
                   posts=tag["posts"])
    # Index of all categories.
    categories = sorted(CATEGORIES.values(), key=lambda c: c["value"].lower())
    build_html("blog/categories/index.html", categories=categories)
    for category in categories:
        build_html(f"blog/categories/{category['name']}/index.html",
                   template="blog/categories/category.html",
                   category=category,
                   posts=category["posts"])
    # All redirected posts.
    for post in REDIRECTED_POSTS:
        build_html(f"{post['path'].strip('/')}/index.html",
                   template="redirect.html", 
                   url=post["redirect"])


def build_pages():
    "Build page files."
    # Site pages.
    for page in PAGES:
        # External pages have no HTML, just links to them.
        if page.get("external"): continue
        path = page["path"].strip("/")
        build_html(f"{path}/index.html",
                   template="page.html", 
                   sitemap=True,
                   page=page,
                   language=page.get("language") or "sv",
                   lastmod=page.get("lastmod"))
    # Deleted, redirected pages.
    for page in REDIRECTED_PAGES:
        path = page["path"].strip("/")
        build_html(f"{path}/index.html",
                   template="redirect.html", 
                   url=page["redirect"])


def build_books():
    "Build book files."
    # Only books having ISBN (possibly a dummy value) are considered.
    books = list(sorted([b for b in BOOKS.values() if b.get("isbn")], 
                        key=lambda b: b["reference"]))
    # Index of all books and their pages.
    build_html("library/index.html", sitemap=True, books=books)
    # References in posts to books.
    for post in POSTS:
        for reference in post.get("references", []):
            BOOKS[reference].setdefault("posts", []).append(post)
    for book in books:
        build_html(book["path"].strip("/"),
                   template="library/book.html",
                   sitemap=True,
                   book=book,
                   language=book.get("language") or "sv",
                   lastmod=book.get("lastmod"))
    # Authors index and pages.
    authors = {}
    for book in books:
        for author in book["authors"]:
            authors.setdefault(author, []).append(book)
    build_html("library/authors/index.html", sitemap=True, authors=authors)
    for author, author_books in authors.items():
        build_html(f"library/authors/{author}/index.html",
                   template="library/authors/author.html",
                   sitemap=True,
                   author=author,
                   books=author_books)
    # Subject index and pages.
    subjects = dict()
    for book in books:
        for subject in book["subjects"]:
            if not subject.strip():
                print(">>> no categories for", book["isbn"], book["title"])
                continue
            subjects.setdefault(subject, []).append(book)
    subjects = list(subjects.items())
    build_html("library/subjects/index.html", sitemap=True, subjects=subjects)
    for subject, subject_books in subjects:
        # Primary sort by published date, secondary sort by authors.
        # Since Python 'sort' is stable, this works.
        subject_books.sort(key=lambda b: b["authors"])
        subject_books.sort(key=lambda b: b["published"], reverse=True)
        build_html(f"library/subjects/{subject}/index.html",
                   template="library/subjects/subject.html",
                   subject=" ".join([p.capitalize() for p in subject.split("-")]),
                   books=subject_books)
    # List of books referred to in posts.
    build_html("library/referred/index.html",
               template="library/referred.html",
               books=[b for b in books if b.get("posts")])
    # List of reviewed books.
    build_html("library/reviewed/index.html",
               template="library/reviewed.html",
               books=[b for b in books if b.get("html")])
    # Lists of books by rating.
    for rating in range(5, 0, -1):
        rated = [b for b in books if b.get("rating") == rating]
        # Primary sort by published date, secondary sort by authors.
        # Since Python 'sort' is stable, this works.
        rated.sort(key=lambda b: b["authors"])
        rated.sort(key=lambda b: b["published"], reverse=True)
        build_html(f"library/rating/{rating}/index.html",
                   template="library/rating.html",
                   rating=rating,
                   books=rated)
    # Lists of books by year published.
    for year, published in sorted(YEAR_PUBLISHED.items()):
        published = sorted(published, key=lambda b: b["reference"])
        build_html(f"library/published/{year}/index.html",
                   template="library/published.html",
                   year=year,
                   books=published)


def build_html(filepath, template=None, pages=None, sitemap=False, **kwargs):
    "Build a single HTML page from the data for an item."
    if template is None:
        template = filepath
    if pages is None:
        pages = PAGES
    template = env.get_template(template)
    # Compute canonical URL.
    canonical = BASE_URL + "/" + filepath
    try: # Remove any trailing "index.html" from canonical URL.
        canonical = canonical[:canonical.index("index.html")]
    except ValueError:
        pass
    kwargs["canonical"] = canonical
    filepath = os.path.join("docs", filepath)
    dirpath = os.path.dirname(filepath)
    if dirpath and not os.path.exists(dirpath):
        os.makedirs(dirpath)
    with open(filepath, "w") as outfile:
        outfile.write(template.render(pages=pages, **kwargs))
    HTML_FILES.add(filepath)
    if sitemap:
        SITEMAP_URLS[canonical] = kwargs.get("lastmod")

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
    result["lastmod"] = time.strftime("%Y-%m-%d", time.gmtime(os.path.getmtime(filepath)))
    return result

def write_rss():
    "Output the RSS feed file."
    template = env.get_template("feed.rss")
    with open("docs/feed.rss", "w") as outfile:
        now = datetime.datetime.now().astimezone()
        pubdate = email.utils.format_datetime(now)
        items = []
        for item in LATEST_ITEMS:
            date = datetime.datetime.fromisoformat(f"{item['date']} 12:00:00").astimezone()
            if item["type"] == "post":
                categories = [c["value"] for c in item.get("categories", [])]
                tags = [t["value"] for t in item.get("tags", [])]
            elif item["type"] == "book":
                categories = ["review"]
                tags = item["subjects"]
            else:
                raise ValueError(f"item {item['title']} has invalid type {item['type']}")
            if item["type"] == "book":
                title = f"{item['authors'][0]}: {item['title']}"
            else:
                title = item["title"]
            item = {"title": title,
                    "url": BASE_URL + item["path"],
                    "category": ", ".join(categories),
                    "description": ", ".join(tags),
                    "pubdate": email.utils.format_datetime(date),
                    "guid": sha1((item["title"] + item["date"]).encode("utf-8")).hexdigest()
            }
            items.append(item)
        outfile.write(template.render(pubdate=pubdate, items=items))

def write_sitemap():
    "Output the sitemap file."
    with open("docs/sitemap.xml", "w") as outfile:
        outfile.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        outfile.write('<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n')
        for url, lastmod in SITEMAP_URLS.items():
            outfile.write("<url>\n")
            outfile.write(f"  <loc>{url}</loc>\n")
            if lastmod:
                outfile.write(f"  <lastmod>{lastmod}</lastmod>\n")
            outfile.write("</url>\n")
        outfile.write("</urlset>\n")

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
    write_rss()
    write_sitemap()
    cleanup_html_files()
