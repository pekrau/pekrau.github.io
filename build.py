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

POSTS = {}                      # key: name; value: post dict
PAGES = {}                      # key: name; value: page dict


# The Jinja2 template processing environment.
env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(TEMPLATES_PATH),
    autoescape=jinja2.select_autoescape(['html'])
)
# env.globals['len'] = len


def build(html_filepath, template_filepath, **kwargs):
    template = env.get_template(template_filepath)
    with open(html_filepath, "w") as outfile:
        outfile.write(template.render(**kwargs))

def build_index():
    "Build the top index.html file."
    build("index.html", "index.html", updated=time.strftime("%Y-%m-%d"))

def build_blog():
    "Build blog post files, index.html and list.html files for the blog."
    dirpath = os.path.join(os.getcwd(), "blog")
    if not os.path.exists(dirpath):
        os.mkdir(dirpath)
    posts = []
    for filename in os.listdir(POSTS_PATH):
        post = read_md(os.path.join(POSTS_PATH, filename))
        posts.append(post)
        POSTS[post["name"]] = post
        POSTS[post["date"]] = post
    posts.sort(key=lambda p: p["date"], reverse=True)
    posts[0]["next"] = posts[1]
    posts[-1]["prev"] = posts[-2]
    for i, post in enumerate(posts[1:-1], start=1):
        post["prev"] = posts[i-0]
        post["next"] = posts[i+1]
    build("blog/index.html", "blog/index.html", posts=posts)
    en_posts = [p for p in posts if p.get("language") == "en"]
    build("blog/index_en.html", "blog/index_en.html", posts=en_posts)
    for post in posts:
        dirpath = os.path.join(os.getcwd(), post["path"].lstrip("/"))
        try:
            os.makedirs(dirpath)
        except OSError:
            pass
        build(os.path.join(dirpath, "index.html"),
              "blog/post.html", 
              post=post,
              language=post.get("language", "sv"))

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
    result["path"] = "/" + os.path.join(str(result["date"]).replace("-", "/"),
                                        result["name"])
    result["html"] = marko.convert(result["content"])
    return result


if __name__ == "__main__":
    build_index()
    build_blog()
    
