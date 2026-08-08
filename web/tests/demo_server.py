#!/usr/bin/env python3
"""Serve PlacementDB synthetic fixtures through representative page routes."""

from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlsplit


WEB_ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ROOT = Path(__file__).resolve().parent / "fixtures"

ROUTES = {
    "/": "home.html",
    "/login": "login.html",
    "/questions": "questions_list.html",
    "/search": "search_unavailable.html",
    "/moderation/queue": "moderation_detail.html",
}


class DemoHandler(SimpleHTTPRequestHandler):
    def translate_path(self, path):
        request_path = urlsplit(path).path
        fixture = ROUTES.get(request_path)
        if fixture:
            return str(FIXTURE_ROOT / fixture)
        if request_path.startswith("/questions/"):
            return str(FIXTURE_ROOT / "question_detail.html")
        if request_path.startswith("/moderation/"):
            return str(FIXTURE_ROOT / "moderation_detail.html")
        if request_path.startswith("/static/"):
            relative = request_path.removeprefix("/static/")
            static_root = (WEB_ROOT / "static").resolve()
            candidate = (static_root / relative).resolve()
            if candidate.is_relative_to(static_root):
                return str(candidate)
        return str(FIXTURE_ROOT / "error.html")

    def do_POST(self):
        self.send_response(303)
        self.send_header("Location", urlsplit(self.path).path or "/")
        self.end_headers()

    def log_message(self, format_string, *args):
        super().log_message(format_string, *args)


def main():
    server = HTTPServer(("127.0.0.1", 8080), DemoHandler)
    print("PlacementDB fixture demo: http://127.0.0.1:8080", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
