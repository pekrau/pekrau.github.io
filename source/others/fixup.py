import yaml
from pathlib import Path

for filepath in Path(".").glob("*.yaml"):
    print(filepath)
    data = yaml.safe_load(filepath.read_text())
    data["published"] = data.pop("date")
    try:
        edition = data.pop("edition")
        data["publisher"] = edition["publisher"]
    except KeyError:
        pass
    try:
        data["href"] = data.pop("url")
    except KeyError:
        pass
    filepath.write_text(yaml.dump(data, allow_unicode=True))
