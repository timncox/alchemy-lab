#!/usr/bin/env python3
"""Find new YouTube videos of the Alchemy Lab and of these firmwares.

    python3 tools/check_videos.py              # list new candidates
    python3 tools/check_videos.py --mark-seen  # ...and remember them as reviewed

Searches YouTube (newest first, no API key: it reads the search page's
ytInitialData), then drops every video already shown on the community hub
(HUB, any YouTube id in its live HTML) or listed in tools/videos-seen.txt. What's left
is printed for a human to judge; nothing is added to the site automatically.
Videos that name one of our firmwares are flagged with *.
"""
import json
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HUB = "https://alchemy-lab-hub.vercel.app/"  # the community index the videos are for
SEEN = ROOT / "tools" / "videos-seen.txt"

# Our instruments, as named on the site. A video is flagged when its title
# names one of these next to "alchemy".
FIRMWARES = ["Mark", "Belt", "Smack", "Seq", "Break", "Stencil", "Tally", "Hypjolin", "Relay",
             "Beads", "Clouds", "Elements", "Marbles", "Plaits", "Warps", "Meld"]

QUERIES = ['"alchemy lab" hermetic', '"alchemy lab" eurorack', '"alchemy lab" firmware',
           '"hermetic modular"', "timncox alchemy"] + [f'"alchemy lab" {n}' for n in FIRMWARES]

# "Alchemy Lab" alone also means a Castlevania level, a game building and a
# perfume house, so a result counts only if its title, channel or description
# snippet says something synth-specific.
RELEVANT = re.compile(r"hermetic modular|eurorack|modular synth|\bmodular\b|daisy|firmware|\bsdk\b"
                      r"|echoa|spagyros|capicola", re.I)
OLD = re.compile(r"\d+\s*(y|years?)\b")  # "5y ago" / "2 years ago": older than the module


def search(query):
    url = "https://www.youtube.com/results?" + urllib.parse.urlencode(
        {"search_query": query, "sp": "CAI="})  # CAI= sorts by upload date
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0", "Accept-Language": "en-US"})
    html = urllib.request.urlopen(req, timeout=20).read().decode("utf-8", "replace")
    m = re.search(r"var ytInitialData = (\{.*?\});</script>", html)
    if not m:
        raise RuntimeError(f"no ytInitialData for {query!r} (YouTube changed its page?)")
    found = []

    def walk(o):
        if isinstance(o, dict):
            v = o.get("videoRenderer")
            if v:
                found.append({
                    "id": v["videoId"],
                    "title": "".join(r["text"] for r in v["title"]["runs"]),
                    "channel": "".join(r["text"] for r in v.get("ownerText", {}).get("runs", [])),
                    "age": v.get("publishedTimeText", {}).get("simpleText", "?"),
                    "snippet": " ".join(r.get("text", "") for s in v.get("detailedMetadataSnippets", [])
                                        for r in s.get("snippetText", {}).get("runs", [])),
                })
            for x in o.values():
                walk(x)
        elif isinstance(o, list):
            for x in o:
                walk(x)

    walk(json.loads(m.group(1)))
    return found


def main():
    mark = "--mark-seen" in sys.argv
    req = urllib.request.Request(HUB, headers={"User-Agent": "Mozilla/5.0"})
    hub = urllib.request.urlopen(req, timeout=20).read().decode("utf-8", "replace")
    known = set(re.findall(r"(?:youtube(?:-nocookie)?\.com/(?:embed/|watch\?v=)|youtu\.be/|i\.ytimg\.com/vi/)([\w-]{11})", hub))
    seen = {l.split()[0] for l in SEEN.read_text().splitlines() if l.strip() and not l.startswith("#")} \
        if SEEN.exists() else set()
    new, errors = {}, []
    for q in QUERIES:
        try:
            try:
                results = search(q)
            except RuntimeError:  # YouTube sometimes serves a page without ytInitialData; one retry
                time.sleep(3)
                results = search(q)
            for v in results:
                if v["id"] in known or v["id"] in seen or v["id"] in new:
                    continue
                if v["age"] == "?" or OLD.search(v["age"]) or \
                        not RELEVANT.search(" ".join((v["title"], v["channel"], v["snippet"]))):
                    continue
                new[v["id"]] = v
        except Exception as e:  # one bad query shouldn't hide the rest
            errors.append(f"{q}: {e}")

    for e in errors:
        print("! " + e, file=sys.stderr)
    if not new:
        print(f"No new videos ({len(known)} on the hub, {len(seen)} reviewed).")
        return 1 if errors else 0
    names = re.compile(r"\b(" + "|".join(FIRMWARES) + r")\b", re.I)
    for v in new.values():
        ours = "*" if re.search(r"alchemy", v["title"], re.I) and names.search(v["title"]) else " "
        print(f"{ours} https://youtu.be/{v['id']}  {v['title']}  — {v['channel']} ({v['age']})")
    print(f"\n{len(new)} new. * = names one of our firmwares.")
    if mark:
        with SEEN.open("a") as f:
            for v in new.values():
                f.write(f"{v['id']}  {v['title']} — {v['channel']}\n")
        print(f"Marked as reviewed in {SEEN.relative_to(ROOT)}.")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
