"Build the website by converting MD to HTML."


__version__ = "0.0.1"


import os
import os.path

import jinja2


TEMPLATES_PATH = os.path.join(os.getcwd(), "templates")


# The Jinja2 template processing environment.
env = jinja2.Environment(
    loader=jinja2.FileSystemLoader(TEMPLATES_PATH),
    autoescape=jinja2.select_autoescape(['html'])
)
env.globals['len'] = len


def build_index():
    "Build the top index.html file."
    template = env.get_template("index.html")
    with open("index.html", "w") as outfile:
        outfile.write(template.render())


if __name__ == "__main__":
    build_index()
