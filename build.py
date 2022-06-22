"Build the website by converting MD to HTML."


__version__ = "0.0.2"


import os
import os.path
import re
import time

import jinja2
import marko
import yaml


FRONT_MATTER_RX = re.compile(r"^---(.*?)---", re.DOTALL | re.MULTILINE)

TEMPLATES_PATH = os.path.join(os.getcwd(), "templates")
POSTS_PATH = os.path.join(os.getcwd(), "source/posts")
PAGES_PATH = os.path.join(os.getcwd(), "source/pages")
FILES_PATH = os.path.join(os.getcwd(), "source/files")

POSTS = []                # sorted by date
PAGES = {}                # key: name; value: page dict
TAGS = {}                 # key: tag; value: number of times used
CATEGORIES = {}           # key: category; value: number of times used


# The Jinja2 template processing environment.
env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(TEMPLATES_PATH),
    autoescape=jinja2.select_autoescape(['html'])
)
env.globals['sorted'] = sorted


def read_posts():
    "Read all Markdown files for blog posts and pre-process."
    for filename in os.listdir(POSTS_PATH):
        post = read_md(os.path.join(POSTS_PATH, filename))
        post["html"] = marko.convert(post["content"])
        POSTS.append(post)
    POSTS.sort(key=lambda p: p["date"], reverse=True)
    POSTS[0]["next"] = POSTS[1]
    POSTS[-1]["prev"] = POSTS[-2]
    for i, post in enumerate(POSTS[1:-1], start=1):
        post["prev"] = POSTS[i-0]
        post["next"] = POSTS[i+1]
    for post in POSTS:
        for tag in post.get("tags", []):
            try:
                TAGS[tag["name"]]["count"] += 1
            except KeyError:
                tag["count"] = 1
                TAGS[tag["name"]] = tag
        for category in post.get("categories", []):
            try:
                CATEGORIES[category["name"]]["count"] += 1
            except KeyError:
                category["count"] = 1
                CATEGORIES[category["name"]] = category

def build_index():
    "Build the top index.html file."
    categories = list(CATEGORIES.values())
    categories.sort(key=lambda c: c["value"].lower())
    build_html("index.html", "index.html",
               updated=time.strftime("%Y-%m-%d"),
               categories=categories)

def build_blog():
    "Build blog post files, index.html and list.html files for the blog."
    dirpath = os.path.join(os.getcwd(), "blog")
    if not os.path.exists(dirpath):
        os.mkdir(dirpath)
    build_html("blog/index.html", "blog/index.html", posts=POSTS)
    en_posts = [p for p in POSTS if p.get("language") == "en"]
    build_html("blog/index_en.html", "blog/index_en.html", posts=en_posts)
    for post in POSTS:
        dirpath = os.path.join(os.getcwd(), post["path"].strip("/"))
        try:
            os.makedirs(dirpath)
        except OSError:
            pass
        build_html(os.path.join(dirpath, "index.html"),
                   "blog/post.html", 
                   post=post,
                   language=post.get("language", "sv"))

def build_html(html_filepath, template_filepath, **kwargs):
    "Build a single HTML page from the data for an item."
    template = env.get_template(template_filepath)
    with open(html_filepath, "w") as outfile:
        outfile.write(template.render(**kwargs))

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
    return result


if __name__ == "__main__":
    read_posts()
    build_index()
    build_blog()
    
