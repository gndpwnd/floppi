#!/usr/bin/env python3
"""
ResearchHub Client — Drop-in API client for external projects.

A single-file, zero-dependency client that can be copied into any project
to interact with a running ResearchHub instance. Uses only Python stdlib.

Configuration (in priority order):
  1. Command-line flags: --url, --workspace
  2. Environment variables: RESEARCHHUB_URL, RESEARCHHUB_WORKSPACE
  3. Config file: .researchhub.json in current directory
  4. Defaults: http://localhost:5347

Usage:
  python researchhub_client.py <command> [options]
  python researchhub_client.py health
  python researchhub_client.py workspace list
  python researchhub_client.py -w my-topic paper add --title "Paper"
  python researchhub_client.py --help

Drop into any project:
  cp researchhub_client.py /path/to/my-project/
  cd /path/to/my-project
  echo '{"url": "http://localhost:5347", "workspace": "my-topic"}' > .researchhub.json
  python researchhub_client.py workspace list
"""

import argparse
import json
import os
import subprocess
import sys
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path


# =============================================================================
# Configuration
# =============================================================================

DEFAULT_URL = "http://localhost:5347"
CONFIG_FILE = ".researchhub.json"


def load_config():
    """Load configuration from file, env vars, with sensible defaults."""
    config = {"url": DEFAULT_URL, "workspace": None}

    # Layer 1: Config file
    config_path = Path.cwd() / CONFIG_FILE
    if config_path.exists():
        try:
            with open(config_path) as f:
                file_config = json.load(f)
            config.update({k: v for k, v in file_config.items() if v is not None})
        except (json.JSONDecodeError, OSError):
            pass

    # Layer 2: Environment variables
    if os.environ.get("RESEARCHHUB_URL"):
        config["url"] = os.environ["RESEARCHHUB_URL"]
    if os.environ.get("RESEARCHHUB_WORKSPACE"):
        config["workspace"] = os.environ["RESEARCHHUB_WORKSPACE"]

    return config


# =============================================================================
# HTTP Client (stdlib only)
# =============================================================================

class APIError(Exception):
    def __init__(self, status, message, body=None):
        self.status = status
        self.message = message
        self.body = body
        super().__init__(f"HTTP {status}: {message}")


def _request(method, url, data=None, timeout=30):
    """Make an HTTP request, return parsed JSON or raw bytes."""
    headers = {"Accept": "application/json"}
    body = None

    if data is not None:
        body = json.dumps(data).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = urllib.request.Request(url, data=body, headers=headers, method=method)

    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read()
            content_type = resp.headers.get("Content-Type", "")
            if "json" in content_type:
                return json.loads(raw)
            return raw
    except urllib.error.HTTPError as e:
        body_text = ""
        try:
            body_text = e.read().decode("utf-8", errors="replace")
        except Exception:
            pass
        raise APIError(e.code, e.reason, body_text)
    except urllib.error.URLError as e:
        raise APIError(0, f"Connection failed: {e.reason}")


def api_get(base_url, path, params=None, timeout=30):
    url = f"{base_url}{path}"
    if params:
        url += "?" + urllib.parse.urlencode(params)
    return _request("GET", url, timeout=timeout)


def api_post(base_url, path, data=None, timeout=30):
    return _request("POST", f"{base_url}{path}", data=data, timeout=timeout)


def api_put(base_url, path, data=None, timeout=30):
    return _request("PUT", f"{base_url}{path}", data=data, timeout=timeout)


def api_delete(base_url, path, timeout=30):
    return _request("DELETE", f"{base_url}{path}", timeout=timeout)


# =============================================================================
# Output Helpers
# =============================================================================

def out_json(data):
    """Pretty-print JSON to stdout."""
    print(json.dumps(data, indent=2, default=str))


def out_table(rows, headers):
    """Print a simple ASCII table."""
    if not rows:
        print("(no results)")
        return

    # Calculate column widths
    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(str(cell)))

    # Print header
    header_line = " | ".join(h.ljust(widths[i]) for i, h in enumerate(headers))
    print(header_line)
    print("-+-".join("-" * w for w in widths))

    # Print rows
    for row in rows:
        print(" | ".join(str(cell).ljust(widths[i]) for i, cell in enumerate(row)))


def out_status(label, value, color=None):
    """Print a labeled status line."""
    print(f"  {label}: {value}")


def die(msg):
    print(f"Error: {msg}", file=sys.stderr)
    sys.exit(1)


# =============================================================================
# Command: health
# =============================================================================

def cmd_health(args, config):
    """Check if ResearchHub is running and healthy."""
    url = config["url"]
    print(f"Checking ResearchHub at {url}...")

    try:
        data = api_get(url, "/api/diagnostics/health")
        print(f"Status: healthy")
        if isinstance(data, dict):
            for k, v in data.items():
                out_status(k, v)
    except APIError as e:
        if e.status == 0:
            die(f"Cannot connect to ResearchHub at {url}")
        print(f"Status: responding (HTTP {e.status})")


# =============================================================================
# Command: workspace
# =============================================================================

def cmd_workspace(args, config):
    url = config["url"]
    sub = args.workspace_cmd

    if sub == "list":
        data = api_get(url, "/api/workspaces")
        # Handle both paginated response and plain list
        if isinstance(data, dict) and "workspaces" in data:
            workspaces = data["workspaces"]
            total = data.get("total", len(workspaces))
        elif isinstance(data, list):
            workspaces = data
            total = len(data)
        else:
            out_json(data)
            return

        rows = []
        for ws in workspaces:
            name = ws.get("name") or ws.get("id", "?")
            topic = ws.get("topic", "")
            state = ws.get("workspace_state", "")
            notes = ws.get("notes_count", 0)
            docs = ws.get("docs_count", 0)
            rows.append([name, topic[:40], state, notes, docs])
        out_table(rows, ["Name", "Topic", "State", "Notes", "Docs"])
        print(f"\nShowing {len(workspaces)} of {total} workspaces")

    elif sub == "create":
        if not args.topic:
            die("--topic is required for workspace create")
        payload = {"topic": args.topic}
        if args.description:
            payload["description"] = args.description
        data = api_post(url, "/api/workspaces", payload)
        print(f"Created workspace: {data.get('id') or data.get('name', '?')}")
        out_json(data)

    elif sub == "detail":
        ws = args.name or config.get("workspace")
        if not ws:
            die("Workspace name required (--name or RESEARCHHUB_WORKSPACE)")
        data = api_get(url, f"/api/workspaces/{ws}")
        out_json(data)

    elif sub == "state":
        ws = args.name or config.get("workspace")
        if not ws:
            die("Workspace name required")
        if args.set_state:
            data = api_put(url, f"/api/workspaces/{ws}/state?state={args.set_state}")
        else:
            data = api_get(url, f"/api/workspaces/{ws}/state")
        out_json(data)

    elif sub == "stats":
        ws = args.name or config.get("workspace")
        if not ws:
            die("Workspace name required")
        data = api_get(url, f"/api/workspaces/{ws}/statistics")
        out_json(data)

    elif sub == "fts-rebuild":
        ws = args.name or config.get("workspace")
        if not ws:
            die("Workspace name required")
        print(f"Rebuilding FTS5 indexes for '{ws}'...")
        data = api_post(url, f"/api/workspaces/{ws}/maintenance/fts-rebuild")
        results = data.get("results", {})
        print(f"  {data.get('message', 'Done')}")
        for table, count in results.items():
            status = f"{count} rows" if count >= 0 else "ERROR"
            print(f"  {table}: {status}")

    elif sub == "absorb":
        target = args.target or config.get("workspace")
        if not target:
            die("Target workspace required (--target or RESEARCHHUB_WORKSPACE)")
        if not args.source:
            die("--source is required")
        payload = {
            "source_workspace": args.source,
            "include_papers": not args.no_papers,
            "include_notes": not args.no_notes,
            "include_pdfs": not args.no_pdfs,
            "include_generated": not args.no_generated,
            "include_research_notes": not getattr(args, "no_notes", False),
            "include_queue": args.include_queue,
            "include_runs": args.include_runs,
            "dry_run": args.dry_run,
            "delete_source_after": args.delete_source,
        }
        data = api_post(url, f"/api/workspaces/{target}/absorb", payload, timeout=120)
        stats = data.get("stats", {})
        if data.get("dry_run"):
            print("DRY RUN — no changes made:")
        else:
            print(f"Absorbed {args.source} -> {target}:")
        rows = [[k.replace("_", " ").title(), str(v)] for k, v in stats.items() if v > 0]
        if rows:
            out_table(rows, ["Metric", "Count"])
        else:
            print("  (nothing to absorb)")
        if data.get("source_deleted"):
            print(f"\nSource workspace '{args.source}' deleted.")

    elif sub == "set-type":
        ws = args.name or config.get("workspace")
        if not ws:
            die("Workspace name required (--name or RESEARCHHUB_WORKSPACE)")
        if not args.type_value:
            die("--type is required (research or test)")
        data = api_put(url, f"/api/workspaces/{ws}/type?workspace_type={args.type_value}")
        print(f"Workspace '{data.get('workspace_name', ws)}' type set to: {data.get('workspace_type')}")

    elif sub == "get-type":
        ws = args.name or config.get("workspace")
        if not ws:
            die("Workspace name required (--name or RESEARCHHUB_WORKSPACE)")
        data = api_get(url, f"/api/workspaces/{ws}/type")
        print(f"Workspace '{data.get('workspace_name', ws)}' type: {data.get('workspace_type')}")

    elif sub == "reset":
        ws = args.name or config.get("workspace")
        if not ws:
            die("Workspace name required (--name or RESEARCHHUB_WORKSPACE)")
        # Build query params
        params = {
            "confirm": "true",
            "keep_papers": str(getattr(args, "keep_papers", True)).lower(),
            "keep_notes": str(getattr(args, "keep_notes", False)).lower(),
            "delete_pdfs": str(getattr(args, "delete_pdfs", False)).lower(),
            "trigger_ingest": str(getattr(args, "trigger_ingest", False)).lower(),
        }
        qs = "&".join(f"{k}={v}" for k, v in params.items())
        print(f"Resetting workspace '{ws}'...")
        print(f"  keep_papers={params['keep_papers']}, keep_notes={params['keep_notes']}, "
              f"delete_pdfs={params['delete_pdfs']}, trigger_ingest={params['trigger_ingest']}")
        data = api_post(url, f"/api/workspaces/{ws}/reset?{qs}", timeout=60)
        tables = data.get("tables_cleared", {})
        total = data.get("total_rows_cleared", 0)
        backup = data.get("backup_path", "")
        errors = data.get("errors", [])
        print(f"\n  Total rows cleared: {total}")
        if backup:
            print(f"  Backup: {backup}")
        if tables:
            rows = [[t, str(c)] for t, c in tables.items() if c > 0]
            if rows:
                out_table(rows, ["Table", "Rows Cleared"])
        if data.get("ingest_triggered"):
            print("  Re-ingestion triggered.")
        if errors:
            print(f"\n  Errors ({len(errors)}):")
            for e in errors:
                print(f"    - {e}")

    else:
        die(f"Unknown workspace command: {sub}")


# =============================================================================
# Command: paper
# =============================================================================

def cmd_paper(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    sub = args.paper_cmd

    if not ws:
        die("Workspace required (--workspace or RESEARCHHUB_WORKSPACE)")

    if sub == "list":
        data = api_get(url, f"/api/workspaces/{ws}/papers")
        if isinstance(data, dict) and "papers" in data:
            papers = data["papers"]
        elif isinstance(data, list):
            papers = data
        else:
            out_json(data)
            return

        rows = []
        for p in papers:
            title = (p.get("title") or "?")[:60]
            doi = p.get("doi") or ""
            year = p.get("year") or ""
            cites = p.get("citation_count")
            cites_str = str(cites) if cites is not None else "-"
            rows.append([title, year, doi[:30], cites_str])
        out_table(rows, ["Title", "Year", "DOI", "Citations"])
        print(f"\nTotal: {len(papers)} papers")

    elif sub == "add":
        if not args.title:
            die("--title is required")
        payload = {"title": args.title}
        if args.authors:
            payload["authors"] = [a.strip() for a in args.authors.split(",")]
        if args.year:
            payload["year"] = args.year
        if args.doi:
            payload["doi"] = args.doi
        data = api_post(url, f"/api/workspaces/{ws}/papers", payload)
        print(f"Added paper to {ws}")
        out_json(data)

    elif sub == "enrich":
        params = {}
        if args.force:
            params["force"] = "true"
        print(f"Enriching papers in {ws}...")
        data = api_post(url, f"/api/workspaces/{ws}/papers/enrich" +
                        ("?force=true" if args.force else ""), timeout=300)
        print(f"Enriched: {data.get('enriched', 0)}, "
              f"Failed: {data.get('failed', 0)}, "
              f"Skipped: {data.get('skipped', 0)}")
        if data.get("details"):
            rows = []
            for d in data["details"][:20]:
                rows.append([
                    d.get("title", "?")[:50],
                    str(d.get("citation_count", "-")),
                    (d.get("venue") or "-")[:30],
                ])
            out_table(rows, ["Title", "Citations", "Venue"])

    elif sub == "search":
        # B65b: REC-13 — paper search via FTS
        q = args.query
        limit = getattr(args, "limit", 20)
        q_encoded = urllib.parse.quote(q, safe="")
        data = api_get(url, f"/api/workspaces/{ws}/papers?q={q_encoded}&limit={limit}")
        if isinstance(data, dict) and "papers" in data:
            papers = data["papers"]
        elif isinstance(data, list):
            papers = data
        else:
            out_json(data)
            return
        rows = []
        for p in papers:
            title = (p.get("title") or "?")[:55]
            authors = ", ".join((p.get("authors") or [])[:2])[:25]
            year = p.get("year") or ""
            doi = (p.get("doi") or "")[:25]
            rows.append([title, year, authors, doi])
        out_table(rows, ["Title", "Year", "Authors", "DOI"])
        print(f"\nFound: {len(papers)} papers matching '{q}'")
        if len(papers) == 0:
            print("Hint: If papers exist but search returns 0, try: workspace fts-rebuild")

    elif sub == "audit":
        # Paper police: audit papers against workspace questions
        if args.all_workspaces:
            # Global audit (no workspace required)
            summary_only = not getattr(args, "detail", False)
            param = "?summary_only=true" if summary_only else ""
            data = api_get(url, f"/api/papers/audit{param}", timeout=120)
            print(f"Paper Police — Global Audit")
            print(f"{'='*60}")
            print(f"  Workspaces scanned:  {data.get('total_workspaces', 0)}")
            print(f"  Total papers:        {data.get('total_papers', 0)}")
            print(f"  Contributing:        {data.get('total_contributing', 0)}")
            print(f"  Orphaned:            {data.get('total_orphaned', 0)}")
            print(f"  Total questions:     {data.get('total_questions', 0)}")
            print(f"  Answered:            {data.get('total_answered', 0)}")
            print(f"  Unanswered:          {data.get('total_unanswered', 0)}")
            rate = data.get('global_contribution_rate', 0)
            print(f"  Contribution rate:   {rate:.1%}")
            print()
            # Per-workspace breakdown
            workspaces = data.get("workspaces", [])
            if workspaces:
                rows = []
                for w in workspaces:
                    rows.append([
                        w.get("workspace", "?")[:30],
                        str(w.get("total_papers", 0)),
                        str(w.get("papers_contributing", 0)),
                        str(w.get("papers_orphaned", 0)),
                        str(w.get("total_questions", 0)),
                        f"{w.get('contribution_rate', 0):.0%}",
                    ])
                out_table(rows, ["Workspace", "Papers", "Contributing", "Orphaned", "Questions", "Rate"])
        else:
            if not ws:
                die("Workspace required (--workspace or --all)")
            data = api_get(url, f"/api/workspaces/{ws}/papers/audit", timeout=60)
            print(f"Paper Police — {ws}")
            print(f"{'='*60}")
            print(f"  Papers:         {data.get('total_papers', 0)}")
            print(f"  Contributing:   {data.get('papers_contributing', 0)}")
            print(f"  Orphaned:       {data.get('papers_orphaned', 0)}")
            print(f"  Questions:      {data.get('total_questions', 0)}")
            print(f"  Answered:       {data.get('questions_answered', 0)}")
            print(f"  Unanswered:     {data.get('questions_unanswered', 0)}")
            rate = data.get('contribution_rate', 0)
            print(f"  Contrib rate:   {rate:.1%}")
            # Show orphaned papers
            orphans = data.get("orphaned_papers", [])
            if orphans:
                print(f"\n  Orphaned papers (top {min(len(orphans), 20)}):")
                for o in orphans[:20]:
                    print(f"    - {o.get('title', '?')[:70]}")
            # Show unanswered questions
            unanswered = data.get("unanswered_questions", [])
            if unanswered:
                print(f"\n  Unanswered questions (top {min(len(unanswered), 10)}):")
                for q in unanswered[:10]:
                    print(f"    ? {q.get('question', '?')[:70]}")

    elif sub == "cleanup":
        # Orphaned paper cleanup
        do_execute = getattr(args, "execute", False)
        confirm_param = "true" if do_execute else "false"

        if args.all_workspaces:
            print(f"Orphaned Paper Cleanup — All Workspaces {'(EXECUTE)' if do_execute else '(dry run)'}")
            print(f"{'='*60}")
            data = api_post(url, f"/api/papers/cleanup?confirm={confirm_param}", timeout=120)
            print(f"  Workspaces scanned:  {data.get('total_workspaces', 0)}")
            print(f"  Total orphaned:      {data.get('total_orphaned', 0)}")
            if do_execute:
                print(f"  Papers deleted:      {data.get('total_deleted', 0)}")
                print(f"  Files deleted:       {data.get('total_files_deleted', 0)}")
            print(f"  Space freed:         {data.get('total_freed_mb', 0):.1f} MB")
            errors = data.get("errors", [])
            if errors:
                print(f"\n  Errors ({len(errors)}):")
                for e in errors[:10]:
                    print(f"    ! {e}")
            # Per-workspace breakdown
            ws_results = data.get("workspaces", [])
            if ws_results:
                rows = []
                for w in ws_results:
                    rows.append([
                        w.get("workspace", "?")[:30],
                        str(w.get("deleted_count", 0)),
                        str(w.get("files_deleted", 0)),
                        f"{w.get('freed_mb', 0):.1f}",
                    ])
                out_table(rows, ["Workspace", "Orphaned", "Files", "MB freed"])
        else:
            if not ws:
                die("Workspace required (--workspace or --all)")
            if do_execute:
                # Delete
                data = api_delete(url, f"/api/workspaces/{ws}/papers/orphaned?confirm=true", timeout=60)
                print(f"Orphaned Paper Cleanup — {ws} (EXECUTE)")
                print(f"{'='*60}")
                print(f"  Papers deleted:  {data.get('deleted_count', 0)}")
                print(f"  Files deleted:   {data.get('files_deleted', 0)}")
                print(f"  Space freed:     {data.get('freed_mb', 0):.1f} MB")
                if data.get("backup_path"):
                    print(f"  Backup:          {data.get('backup_path')}")
                errors = data.get("errors", [])
                if errors:
                    print(f"\n  Errors ({len(errors)}):")
                    for e in errors[:10]:
                        print(f"    ! {e}")
            else:
                # Preview
                data = api_get(url, f"/api/workspaces/{ws}/papers/orphaned", timeout=60)
                print(f"Orphaned Paper Preview — {ws}")
                print(f"{'='*60}")
                print(f"  Orphaned papers:   {data.get('orphaned_count', 0)}")
                print(f"  Deletable PDFs:    {data.get('deletable_count', 0)}")
                print(f"  DB-only removals:  {data.get('db_only_count', 0)}")
                print(f"  Total size:        {data.get('total_size_mb', 0):.1f} MB")
                orphans = data.get("orphaned_papers", [])
                if orphans:
                    print(f"\n  Orphaned papers (top {min(len(orphans), 20)}):")
                    for o in orphans[:20]:
                        managed = " [PDF]" if o.get("in_managed_folder") else ""
                        print(f"    - {o.get('title', '?')[:65]}{managed}")
                if data.get("orphaned_count", 0) > 0:
                    print(f"\n  Run with --execute to delete these papers.")

    else:
        die(f"Unknown paper command: {sub}")


# =============================================================================
# Command: note
# =============================================================================

def cmd_note(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    sub = args.note_cmd

    if not ws:
        die("Workspace required (--workspace or RESEARCHHUB_WORKSPACE)")

    if sub == "list":
        params = {}
        if args.type:
            params["note_type"] = args.type
        data = api_get(url, f"/api/workspaces/{ws}/notes", params=params)
        notes = data if isinstance(data, list) else data.get("notes", [])
        rows = []
        for n in notes:
            rows.append([
                n.get("id", "?")[:12],
                n.get("note_type", ""),
                (n.get("title") or "?")[:50],
                (n.get("tags") or "")[:20] if isinstance(n.get("tags"), str)
                else ",".join(n.get("tags", []))[:20],
            ])
        out_table(rows, ["ID", "Type", "Title", "Tags"])
        print(f"\nTotal: {len(notes)} notes")

    elif sub == "generate":
        source = args.source or "papers"
        payload = {"source": source}
        if args.max_sources:
            payload["max_sources"] = args.max_sources
        data = api_post(url, f"/api/workspaces/{ws}/notes/generate", payload)
        print(f"Generation job started: {data.get('job_id', '?')}")
        print(f"Status: {data.get('status', '?')}")
        if data.get("job_id"):
            print(f"\nPoll with: python {sys.argv[0]} note status "
                  f"--workspace {ws} --job-id {data['job_id']}")

    elif sub == "status":
        if not args.job_id:
            die("--job-id required")
        data = api_get(url, f"/api/workspaces/{ws}/notes/generate/{args.job_id}")
        # B58c: Show skip reasons if present
        print(f"Job: {data.get('id', args.job_id)}")
        print(f"Status: {data.get('status', '?')}")
        print(f"Notes created: {data.get('notes_created', 0)}")
        print(f"Sources processed: {data.get('sources_processed', 0)}")
        if data.get("error"):
            print(f"Error: {data['error']}")
        skip = data.get("skip_reasons", {})
        if skip:
            print("Skip reasons:")
            if skip.get("already_processed", 0) > 0:
                print(f"  Already processed: {skip['already_processed']}")
            if skip.get("llm_errors", 0) > 0:
                print(f"  LLM batch errors: {skip['llm_errors']}")
            if skip.get("insufficient_content", 0) > 0:
                print(f"  Insufficient content: {skip['insufficient_content']}")
        if data.get("batch_errors"):
            print("Batch errors:")
            for err in data["batch_errors"]:
                print(f"  - {err}")

    elif sub == "search":
        if not args.query:
            die("--query required")
        data = api_get(url, f"/api/workspaces/{ws}/notes/search",
                       params={"q": args.query})
        out_json(data)

    elif sub == "relink":
        print(f"Re-linking notes in '{ws}'...")
        data = api_post(url, f"/api/workspaces/{ws}/notes/relink", {}, timeout=120)
        print(f"Notes processed: {data.get('notes_processed', 0)}")
        print(f"Links created: {data.get('links_created', 0)}")

    else:
        die(f"Unknown note command: {sub}")


# =============================================================================
# Command: cite (citation export)
# =============================================================================

def cmd_cite(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    sub = args.cite_cmd

    if not ws:
        die("Workspace required (--workspace or RESEARCHHUB_WORKSPACE)")

    if sub == "export":
        fmt = args.format or "bibtex"
        if fmt not in ("bibtex", "ris", "endnote"):
            die(f"Invalid format: {fmt}. Use: bibtex, ris, endnote")

        if args.output:
            # Download to file
            data = api_get(url, f"/api/workspaces/{ws}/citations/export/download",
                           params={"format": fmt})
            with open(args.output, "wb") as f:
                f.write(data if isinstance(data, bytes) else data.encode("utf-8"))
            print(f"Exported to {args.output}")
        else:
            # Print to stdout
            data = api_get(url, f"/api/workspaces/{ws}/citations/export",
                           params={"format": fmt})
            if isinstance(data, dict):
                print(data.get("content", ""))
                print(f"\n({data.get('paper_count', 0)} papers exported as {fmt})")
            else:
                print(data)

    elif sub == "stats":
        data = api_get(url, f"/api/workspaces/{ws}/citations/stats")
        out_json(data)

    else:
        die(f"Unknown cite command: {sub}")


# =============================================================================
# Command: source (source folders)
# =============================================================================

def cmd_source(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    sub = args.source_cmd

    if not ws:
        die("Workspace required (--workspace or RESEARCHHUB_WORKSPACE)")

    if sub == "list":
        data = api_get(url, f"/api/workspaces/{ws}/sources/folders")
        folders = data.get("folders", data) if isinstance(data, dict) else data
        if isinstance(folders, list) and folders:
            rows = []
            for f in folders:
                rows.append([
                    f.get("label", ""),
                    f.get("path", ""),
                    f.get("sync_mode", ""),
                    str(f.get("files_indexed", 0)),
                    "yes" if f.get("folder_exists", True) else "NO",
                ])
            out_table(rows, ["Label", "Path", "Mode", "Files", "Exists"])
        else:
            print("No source folders configured.")

    elif sub == "add":
        if not args.path:
            die("--path is required")
        payload = {"path": args.path}
        if args.label:
            payload["label"] = args.label
        if args.pattern:
            payload["file_patterns"] = [args.pattern]
        if args.mode:
            payload["sync_mode"] = args.mode
        data = api_post(url, f"/api/workspaces/{ws}/sources/folders", payload)
        print(f"Added source folder: {args.path}")
        out_json(data)

    elif sub == "scan":
        data = api_post(url, f"/api/workspaces/{ws}/sources/scan")
        out_json(data)

    elif sub == "ingest":
        params = {}
        if args.max_files:
            params["max_files"] = str(args.max_files)
        if args.force:
            params["force"] = "true"
        query = "&".join(f"{k}={v}" for k, v in params.items())
        path = f"/api/workspaces/{ws}/sources/ingest"
        if query:
            path += f"?{query}"
        print(f"Ingesting files in {ws} (this may take a while)...")
        data = api_post(url, path, timeout=600)
        out_json(data)

    elif sub == "sync":
        data = api_post(url, f"/api/workspaces/{ws}/sources/sync")
        out_json(data)

    else:
        die(f"Unknown source command: {sub}")


# =============================================================================
# Command: research
# =============================================================================

def cmd_research(args, config):
    url = config["url"]
    ws = config.get("workspace")
    sub = args.research_cmd

    # Old integration commands
    if sub == "submit":
        if not args.topic:
            die("--topic is required")
        payload = {"topic": args.topic}
        if ws:
            payload["workspace_id"] = ws
        if args.depth:
            payload["depth"] = args.depth
        data = api_post(url, "/api/integration/submit", payload)
        print("Research job submitted:")
        if ws:
            print(f"  Workspace: {ws}")
        job_id = data.get("job_id", data.get("id", ""))
        if job_id:
            print(f"  Job ID: {job_id}")
            print(f"  Check status: research status --job-id {job_id}")
        print(f"  Output: generated/ folder in workspace path (auto-synced)")
        out_json(data)
        return

    # Commands that don't require a workspace
    if sub in ("metrics", "overview", "complexity", "completeness"):
        # Handled below, no workspace needed
        pass
    elif not ws:
        # B34 commands require workspace
        die("Workspace required (--workspace or RESEARCHHUB_WORKSPACE)")

    if sub == "execute":
        payload = {"mode": args.mode}
        if args.max:
            payload["max_topics"] = args.max
        if args.priority:
            payload["priority_filter"] = args.priority
        if args.depth:
            payload["depth"] = args.depth
        if args.force:
            payload["force"] = True
        if args.topics:
            payload["topic_ids"] = [t.strip() for t in args.topics.split(",")]
        data = api_post(url, f"/api/workspaces/{ws}/research/execute", payload, timeout=120)
        print(f"Execution started: {data.get('execution_id', 'unknown')}")
        out_json(data)

    elif sub == "status":
        if getattr(args, "list", False):
            # B49i: List recent runs as table
            data = api_get(url, f"/api/workspaces/{ws}/research/runs?limit=20")
            runs = data.get("runs", data) if isinstance(data, dict) else data
            if isinstance(runs, list) and runs:
                rows = []
                for r in runs:
                    rows.append([
                        r.get("run_id", r.get("id", ""))[:12],
                        r.get("mode", ""),
                        r.get("status", ""),
                        r.get("topic_name", r.get("topic", ""))[:40],
                        (r.get("started_at") or r.get("created_at") or "")[:19],
                    ])
                out_table(rows, ["Run ID", "Mode", "Status", "Topic", "Started"])
            else:
                print("No research runs found.")
        else:
            # B53f: Support both positional and --execution-id flag
            exec_id = getattr(args, "id", None) or getattr(args, "execution_id_flag", None)
            if exec_id:
                data = api_get(url, f"/api/workspaces/{ws}/research/execute/{exec_id}")
                out_json(data)
            elif args.job_id:
                # Legacy integration status
                data = api_get(url, f"/api/integration/status/{args.job_id}")
                out_json(data)
            else:
                die("Usage: research status <execution-id> or --execution-id <id> or --job-id <id> or --list")

    elif sub == "cancel":
        if not args.run_id:
            die("--run-id is required")
        data = api_post(url, f"/api/workspaces/{ws}/research/runs/{args.run_id}/cancel")
        print(f"Run {args.run_id}: {data.get('status', 'unknown')}")
        out_json(data)

    elif sub == "pause":
        if not args.run_id:
            die("--run-id is required")
        data = api_post(url, f"/api/workspaces/{ws}/research/runs/{args.run_id}/pause")
        print(f"Run {args.run_id}: {data.get('status', 'unknown')}")
        out_json(data)

    elif sub == "resume":
        if not args.run_id:
            die("--run-id is required")
        data = api_post(url, f"/api/workspaces/{ws}/research/runs/{args.run_id}/resume")
        print(f"Run {args.run_id}: {data.get('status', 'unknown')}")
        out_json(data)

    elif sub == "active":
        data = api_get(url, f"/api/workspaces/{ws}/research/active")
        total = data.get("total", 0)
        print(f"Active research runs: {total}")
        for category in ["active_runs", "paused_runs", "queued_runs"]:
            runs = data.get(category, [])
            if runs:
                label = category.replace("_", " ").title()
                print(f"\n{label}:")
                rows = []
                for r in runs:
                    rows.append([
                        r.get("run_id", "")[:12],
                        r.get("mode", ""),
                        r.get("status", ""),
                        f"{r.get('progress_percent', 0):.0f}%",
                        r.get("phase", ""),
                    ])
                out_table(rows, ["Run ID", "Mode", "Status", "Progress", "Phase"])

    elif sub == "runs":
        if not args.topic_id:
            die("--topic-id is required")
        data = api_get(url, f"/api/workspaces/{ws}/queue/{args.topic_id}/runs")
        print(f"Topic: {data.get('topic', 'unknown')}")
        print(f"Modes completed: {', '.join(data.get('modes_completed', []))}")
        print(f"Modes pending: {', '.join(data.get('modes_pending', []))}")
        runs = data.get("runs", [])
        if runs:
            rows = []
            for r in runs:
                rows.append([
                    r.get("run_id", "")[:12],
                    r.get("mode", ""),
                    r.get("status", ""),
                    f"{r.get('progress_percent', 0):.0f}%",
                    str(r.get("papers_found", 0)),
                    str(r.get("web_sources_found", 0)),
                ])
            out_table(rows, ["Run ID", "Mode", "Status", "Progress", "Papers", "Web"])

    elif sub == "queue":
        action = args.action
        if action == "list":
            data = api_get(url, f"/api/workspaces/{ws}/queue")
            topics = data.get("items", data.get("topics", data)) if isinstance(data, dict) else data
            if isinstance(topics, list) and topics:
                rows = []
                for t in topics:
                    rows.append([
                        t.get("id", "")[:12],
                        t.get("topic", "")[:50],
                        str(t.get("priority", "")),
                        t.get("status", ""),
                        ", ".join(t.get("modes_completed", [])),
                    ])
                out_table(rows, ["ID", "Topic", "Priority", "Status", "Completed"])
            else:
                print("Queue is empty")
        elif action == "add":
            if not args.topic:
                die("--topic is required for queue add")
            payload = {"topic": args.topic, "priority": args.priority}
            data = api_post(url, f"/api/workspaces/{ws}/queue", payload)
            print(f"Added to queue: {data.get('topic', args.topic)}")
            out_json(data)
        elif action == "remove":
            if not args.topic_id:
                die("--topic-id is required for queue remove")
            data = api_delete(url, f"/api/workspaces/{ws}/queue/{args.topic_id}")
            print(f"Removed topic {args.topic_id}")
        elif action == "set-priority":
            if not args.topic_id:
                die("--topic-id is required for queue set-priority")
            params = {}
            if args.level:
                params["level"] = args.level
            elif args.priority:
                params["value"] = args.priority
            else:
                die("--level or --priority is required for queue set-priority")
            qs = "&".join(f"{k}={v}" for k, v in params.items())
            data = api_put(url, f"/api/workspaces/{ws}/queue/{args.topic_id}/priority?{qs}")
            print(f"Priority set: {data.get('topic', '')} → {data.get('priority', '')}")
            out_json(data)
        elif action == "stats":
            data = api_get(url, f"/api/workspaces/{ws}/queue/stats")
            out_json(data)

    elif sub == "query":
        # Cross-workspace RAG query via global endpoint
        payload = {
            "query": args.question,
            "top_k": getattr(args, "top_k", 5),
            "similarity_threshold": getattr(args, "threshold", 0.5),
            "method": "rrf",
        }
        # Limit to specific workspace if explicitly requested via --workspace flag
        query_ws = getattr(args, "query_workspace", None)
        if query_ws:
            payload["workspace_ids"] = [query_ws]
        data = api_post(url, "/api/rag/query/global", payload)
        results = data.get("results", [])
        if results:
            print(f"Found {len(results)} result(s) for: {args.question}\n")
            for i, r in enumerate(results, 1):
                ws_name = r.get("workspace_name", r.get("workspace_id", "unknown"))
                rrf = r.get("rrf_score", 0)
                sim = r.get("combined_score", rrf)
                fpath = r.get("file_path", "unknown")
                content = r.get("content", "")[:200].replace("\n", " ").strip()
                print(f"[{i}] {ws_name}  (score: {sim:.3f})")
                print(f"    File: {fpath}")
                print(f"    {content}...")
                print()
        else:
            print("No results found. Try rephrasing your query or lowering --threshold.")

    elif sub == "questions":
        # B57/B58: Detect open questions in workspace documents
        payload = {}
        if getattr(args, "file", None):
            payload["file_path"] = args.file
        print(f"Detecting open questions in '{ws}'...")
        data = api_post(url, f"/api/workspaces/{ws}/questions/detect", payload, timeout=120)
        print(f"Files scanned: {data.get('files_scanned', 0)}")
        print(f"Questions found: {data.get('total_questions', 0)}")
        print(f"  Rhetorical: {data.get('rhetorical', 0)}")
        print(f"  Specific: {data.get('specific', 0)}")
        print(f"  Vague: {data.get('vague', 0)}")
        print(f"Queued: {data.get('queued', 0)}")
        questions = data.get("questions", [])
        if questions:
            print("\nDetected questions:")
            for q in questions[:10]:
                cls = q.get("classification", "?")
                text = q.get("text", "?")[:80]
                topic = q.get("suggested_topic", "")
                print(f"  [{cls}] {text}")
                if topic:
                    print(f"         -> Topic: {topic}")

    elif sub == "provenance":
        tid = args.topic_id
        if not tid:
            die("--topic-id is required")
        data = api_get(url, f"/api/workspaces/{ws}/queue/{tid}/provenance")
        summary = data.get("summary", {})
        print(f"Topic: {data.get('topic', '?')}")
        print(f"Runs: {summary.get('total_runs', 0)}")
        print(f"Papers: {summary.get('total_papers', 0)}")
        print(f"Notes: {summary.get('total_notes', 0)}")
        print(f"Findings docs: {summary.get('total_findings_docs', 0)}")
        print(f"Modes completed: {', '.join(summary.get('modes_completed', []))}")
        papers = data.get("papers", [])
        if papers:
            rows = []
            for p in papers:
                title = (p.get("title") or "?")[:50]
                authors = ", ".join((p.get("authors") or [])[:2])[:25]
                year = p.get("year") or ""
                rows.append([title, year, authors])
            print(f"\nPapers ({len(papers)}):")
            out_table(rows, ["Title", "Year", "Authors"])
        notes = data.get("notes", [])
        if notes:
            rows = []
            for n in notes:
                title = (n.get("title") or "?")[:50]
                ntype = n.get("note_type") or "?"
                rows.append([title, ntype])
            print(f"\nNotes ({len(notes)}):")
            out_table(rows, ["Title", "Type"])

    elif sub == "metrics":
        # Cross-workspace research metrics (no workspace required)
        try:
            data = api_get(config["url"], "/api/metrics/research")
        except APIError as e:
            if e.status == 404:
                print("Research metrics endpoint not available (404).")
                print("This feature requires the /api/metrics/research endpoint.")
                return
            raise

        totals = data.get("totals", {})
        print("=== Research Metrics ===\n")
        print(f"  Total papers:     {totals.get('papers', 0):,}")
        print(f"  Total notes:      {totals.get('notes', 0):,}")
        print(f"  Total topics:     {totals.get('topics', 0):,}")
        print(f"  Total runs:       {totals.get('runs', 0):,}")
        print(f"  Total workspaces: {totals.get('workspaces', 0):,}")
        print()

        workspaces = data.get("workspaces", [])
        if workspaces:
            rows = []
            for w in workspaces:
                completed = w.get("topics_completed", 0)
                total_t = w.get("topics_total", 0)
                topics_str = f"{completed}/{total_t}"
                rows.append([
                    w.get("name", "?"),
                    str(w.get("papers", 0)),
                    str(w.get("notes", 0)),
                    topics_str,
                    str(w.get("runs", 0)),
                ])
            out_table(rows, ["Workspace", "Papers", "Notes", "Topics (done/total)", "Runs"])
        else:
            print("No workspace data available.")

    elif sub == "complexity":
        # Research complexity growth metrics (no workspace required)
        try:
            data = api_get(config["url"], "/api/stats/complexity")
        except APIError as e:
            if e.status == 404:
                print("Complexity metrics endpoint not available (404).")
                return
            raise

        g = data.get("global", {})
        print("=== Research Complexity Growth ===\n")
        print(f"  Total topics:     {g.get('total_topics', 0):,}")
        print(f"  Completed:        {g.get('total_completed', 0):,}")
        print(f"  Completion:       {g.get('completion_pct', 0):.1f}%")
        print()
        print(f"  7-day:  +{g.get('topics_added_7d', 0)} added, "
              f"+{g.get('topics_completed_7d', 0)} completed, "
              f"net {g.get('net_progress_7d', 0):+d}")
        print(f"  30-day: +{g.get('topics_added_30d', 0)} added, "
              f"+{g.get('topics_completed_30d', 0)} completed, "
              f"net {g.get('net_progress_30d', 0):+d}")
        print()

        workspaces = data.get("workspaces", [])
        if workspaces:
            rows = []
            for w in workspaces:
                cur = w.get("current", {})
                h7 = w.get("history_7d", {})
                h30 = w.get("history_30d", {})
                growing = "YES" if h7.get("complexity_growing") else "no"
                rows.append([
                    w.get("name", "?"),
                    f"{cur.get('completed_topics', 0)}/{cur.get('total_topics', 0)}",
                    f"{cur.get('completion_pct', 0):.1f}%",
                    f"+{h7.get('topics_added', 0)}/-{h7.get('topics_completed', 0)}",
                    f"{h7.get('net_progress', 0):+d}",
                    growing,
                ])
            out_table(rows, ["Workspace", "Done/Total", "Pct", "7d Add/Done", "7d Net", "Growing?"])

        if getattr(args, "json", False):
            out_json(data)

    elif sub == "completeness":
        # Dual-completeness: answer vs academic quality (no workspace required)
        try:
            data = api_get(config["url"], "/api/stats/completeness-quality")
        except APIError as e:
            if e.status == 404:
                print("Completeness quality endpoint not available (404).")
                return
            raise

        print("=== Research Completeness Quality ===\n")
        print(f"  Total questions:       {data.get('total_questions', 0):,}")
        print(f"  Answer completeness:   {data.get('answer_completeness', 0):.1%}")
        print(f"  Academic completeness: {data.get('academic_completeness', 0):.1%}")
        print(f"  Web-only answers:      {data.get('web_only_answers', 0):.1%}")
        print()

        sb = data.get("source_breakdown", {})
        print(f"  Academic-backed: {sb.get('academic_backed', 0):,}")
        print(f"  Web-only:        {sb.get('web_only', 0):,}")
        print(f"  Unanswered:      {sb.get('unanswered', 0):,}")
        print()

        workspaces = data.get("workspaces", [])
        if workspaces:
            rows = []
            for w in workspaces:
                rows.append([
                    w.get("workspace", "?"),
                    str(w.get("total_questions", 0)),
                    f"{w.get('answer_completeness', 0):.0%}",
                    f"{w.get('academic_completeness', 0):.0%}",
                    f"{w.get('web_only_pct', 0):.0%}",
                    str(w.get("unanswered", 0)),
                ])
            out_table(rows, ["Workspace", "Questions", "Answered", "Academic", "Web-only", "Unanswered"])

        if getattr(args, "json", False):
            out_json(data)

    elif sub == "overview":
        try:
            data = api_get(url, "/api/research/system-status")
        except APIError as e:
            if e.status == 404:
                die("Research status endpoint not available (requires ResearchHub with research_status router)")
            raise

        state = data.get("state", "unknown")
        state_icons = {
            "researching": "[ACTIVE]",
            "waiting": "[WAITING]",
            "paused": "[PAUSED]",
            "idle": "[IDLE]",
            "error": "[ERROR]",
        }
        icon = state_icons.get(state, "[?]")
        print(f"\n  {icon} Research State: {state.upper()}\n")

        # Active session details
        details = data.get("details")
        if details:
            print("  Active Session:")
            print(f"    Workspace:    {details.get('current_workspace', '?')}")
            fill = details.get('fill_level', 0)
            tokens = details.get('tokens_used', 0)
            ctx = details.get('context_window', 0)
            print(f"    Token usage:  {tokens:,} / {ctx:,} ({fill:.1%} full)")
            print(f"    Requests:     {details.get('requests_made', 0)}")
            print(f"    Questions:    {details.get('questions_answered', 0)} answered / "
                  f"{details.get('questions_researched', 0)} attempted")
            print()

        # Auto-research info
        ar = data.get("auto_research", {})
        if ar.get("enabled"):
            print("  Auto-Research:")
            if ar.get("executing"):
                print("    Status:       Executing cycle now")
            elif ar.get("next_cycle_in_seconds") is not None:
                mins = ar["next_cycle_in_seconds"] // 60
                secs = ar["next_cycle_in_seconds"] % 60
                print(f"    Next cycle:   {mins}m {secs}s")
            last = ar.get("last_cycle_at")
            if last:
                print(f"    Last cycle:   {last[:19]}")
            print(f"    Repos:        {ar.get('repos_configured', 0)} configured")
            stats = ar.get("last_cycle_stats")
            if stats:
                print(f"    Last stats:   {stats.get('research_started', 0)} started, "
                      f"{stats.get('repos_processed', 0)} repos, "
                      f"{stats.get('deferred', 0)} deferred")
            print()
        else:
            print("  Auto-Research: disabled\n")

        # Resources
        res = data.get("resources", {})
        print("  Resources:")
        ollama = res.get("ollama_available")
        print(f"    Ollama:       {'available' if ollama else 'UNAVAILABLE' if ollama is False else 'unknown'}")
        if res.get("models_loaded"):
            print(f"    Models:       {', '.join(res['models_loaded'])}")
        if res.get("context_window"):
            print(f"    Context:      {res['context_window']:,} tokens")
        if res.get("gpu_memory_free_mb") is not None:
            print(f"    GPU free:     {res['gpu_memory_free_mb']:,} MB")
        if res.get("memory_pressure"):
            print("    VRAM:         PRESSURE (< 1 GB free)")
        print()

        # Blockers
        blockers = data.get("blockers", [])
        if blockers:
            print("  Blockers:")
            for b in blockers:
                print(f"    - {b}")
            print()

        # Monitors
        monitors = data.get("monitors", {})
        if monitors:
            rows = [[name, status] for name, status in sorted(monitors.items())]
            out_table(rows, ["Monitor", "Status"])
            print()

        if getattr(args, "json", False):
            out_json(data)

    elif sub == "evidence":
        if getattr(args, "question_id", None):
            # Single question evidence chain
            data = api_get(url, f"/api/workspaces/{ws}/questions/{args.question_id}/evidence-chain")
            print(f"\nQuestion: {data.get('question', '?')}")
            print(f"Status: {data.get('status', '?')}")
            print(f"Score: {data.get('answer_score', 'N/A')}")
            chain = data.get("evidence_chain", {})
            print(f"Source quality: {chain.get('source_quality', 'none')}")

            refs = chain.get("references", [])
            if refs:
                print(f"\nReferences ({len(refs)}):")
                rows = []
                for r in refs:
                    title = (r.get("title") or "?")[:50]
                    rtype = r.get("type", "paper")
                    rel = f"{r.get('relevance', 0):.2f}"
                    rows.append([title, rtype, rel, r.get("match_type", "")])
                out_table(rows, ["Title", "Type", "Relevance", "Match"])

            notes = chain.get("notes", [])
            if notes:
                print(f"\nNotes ({len(notes)}):")
                rows = []
                for n in notes:
                    title = (n.get("title") or "?")[:50]
                    ntype = n.get("note_type") or "?"
                    rows.append([title, ntype, (n.get("source_paper_id") or "")[:12]])
                out_table(rows, ["Title", "Type", "Source Paper"])

            answer = chain.get("answer_summary")
            if answer:
                print(f"\nAnswer summary:\n  {answer[:500]}")
            elif not refs:
                print("\nNo evidence found for this question.")

            if getattr(args, "json", False):
                out_json(data)

        elif getattr(args, "show_all", False):
            # Bulk evidence chains
            params = "?limit=100"
            if getattr(args, "status", None):
                params += f"&status={args.status}"
            data = api_get(url, f"/api/workspaces/{ws}/evidence-chains{params}")
            total = data.get("total_questions", 0)
            answered = data.get("answered", 0)
            unanswered = data.get("unanswered", 0)
            print(f"\n=== Evidence Chains for '{ws}' ===")
            print(f"Total questions: {total}  (answered: {answered}, unanswered: {unanswered})\n")

            questions = data.get("questions", [])
            if questions:
                rows = []
                for q in questions:
                    chain = q.get("evidence_chain", {})
                    refs_count = len(chain.get("references", []))
                    notes_count = len(chain.get("notes", []))
                    quality = chain.get("source_quality", "none")
                    qtext = (q.get("question") or "?")[:55]
                    rows.append([
                        qtext,
                        q.get("status", "?"),
                        str(refs_count),
                        str(notes_count),
                        quality,
                    ])
                out_table(rows, ["Question", "Status", "Refs", "Notes", "Quality"])
            else:
                print("No questions found.")

            if getattr(args, "json", False):
                out_json(data)

        else:
            die("Usage: research evidence --question-id <ID> or research evidence --all [--status answered|unanswered|partial]")

    elif sub == "results":
        if not args.job_id:
            die("--job-id is required")
        data = api_get(url, f"/api/integration/results/{args.job_id}")
        out_json(data)

    else:
        die(f"Unknown research command: {sub}")


# =============================================================================
# Command: upload (B45c — file upload + sort subcommands)
# =============================================================================

def _upload_file(url, ws, filepath, subfolder=None):
    """Upload a single file to a workspace. Returns parsed response."""
    import mimetypes
    filename = os.path.basename(filepath)
    boundary = "----ResearchHubClientBoundary"
    content_type = mimetypes.guess_type(filepath)[0] or "application/octet-stream"

    with open(filepath, "rb") as f:
        file_data = f.read()

    body_parts = []
    body_parts.append(f"--{boundary}".encode())
    body_parts.append(
        f'Content-Disposition: form-data; name="file"; filename="{filename}"'.encode()
    )
    body_parts.append(f"Content-Type: {content_type}".encode())
    body_parts.append(b"")
    body_parts.append(file_data)
    body_parts.append(f"--{boundary}--".encode())
    body = b"\r\n".join(body_parts)

    upload_url = f"{url}/api/workspaces/{ws}/upload"
    if subfolder:
        upload_url += f"?subfolder={urllib.parse.quote(subfolder)}"

    req = urllib.request.Request(
        upload_url,
        data=body,
        headers={
            "Content-Type": f"multipart/form-data; boundary={boundary}",
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        body_text = e.read().decode("utf-8", errors="replace")
        die(f"Upload failed (HTTP {e.code}): {body_text[:200]}")


def cmd_upload(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    sub = args.upload_cmd

    if not ws:
        die("Workspace required (--workspace or RESEARCHHUB_WORKSPACE)")

    if sub == "file":
        filepath = args.file
        if not filepath or not os.path.exists(filepath):
            die(f"File not found: {filepath}")
        subfolder = args.subfolder  # None → server defaults to uploads/
        result = _upload_file(url, ws, filepath, subfolder=subfolder)
        print(f"Uploaded: {os.path.basename(filepath)}")
        if subfolder:
            print(f"  Destination: {subfolder}/")
        else:
            print(f"  Destination: uploads/ (staging)")
        out_json(result)

    elif sub == "sort":
        params = {}
        if args.dry_run:
            params["dry_run"] = "true"
        query = "&".join(f"{k}={v}" for k, v in params.items())
        path = f"/api/workspaces/{ws}/uploads/sort"
        if query:
            path += f"?{query}"
        data = api_post(url, path, timeout=120)
        sorted_count = data.get("sorted", 0)
        is_dry = data.get("dry_run", False)

        if is_dry:
            print(f"Dry run — would sort {sorted_count} file(s):")
        else:
            print(f"Sorted {sorted_count} file(s):")

        for d in data.get("details", []):
            action = d.get("action", "?")
            name = d.get("file", "?")
            if action in ("moved", "would_move"):
                dest = d.get("to", "?")
                print(f"  {name} → {dest}/")
            elif action == "skipped":
                reason = d.get("reason", "")
                print(f"  {name} (skipped: {reason})")

    else:
        die(f"Unknown upload command: {sub}. Use: file, sort")


# =============================================================================
# Command: docs (project documentation sync to RAG)
# =============================================================================

def _resolve_doc_paths(config):
    """Resolve scope_files, roadmap_files, additional_docs from config.

    Returns list of (filepath: Path, doc_type: str) tuples.
    Resolves relative paths against project_root (from config) or cwd.
    """
    project_root = Path(config.get("project_root", "."))
    if not project_root.is_absolute():
        project_root = Path.cwd() / project_root
    project_root = project_root.resolve()

    doc_files = []

    # scope_files (plural) takes precedence over scope_file (singular)
    scope_files = config.get("scope_files", [])
    scope_file = config.get("scope_file")
    if scope_files:
        for sf in scope_files:
            p = Path(sf) if os.path.isabs(sf) else project_root / sf
            doc_files.append((p.resolve(), "scope"))
    elif scope_file:
        p = Path(scope_file) if os.path.isabs(scope_file) else project_root / scope_file
        doc_files.append((p.resolve(), "scope"))

    for rf in config.get("roadmap_files", []):
        p = Path(rf) if os.path.isabs(rf) else project_root / rf
        doc_files.append((p.resolve(), "roadmap"))

    for ad in config.get("additional_docs", []):
        p = Path(ad) if os.path.isabs(ad) else project_root / ad
        doc_files.append((p.resolve(), "additional"))

    return doc_files


def cmd_docs(args, config):
    """Project documentation sync to workspace RAG."""
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    sub = args.docs_cmd

    if not ws:
        die("Workspace required (--workspace or RESEARCHHUB_WORKSPACE)")

    if sub == "sync":
        # Collect doc file paths from config + CLI overrides
        doc_files = _resolve_doc_paths(config)

        # CLI --file flags add extra files
        extra_files = getattr(args, "file", None) or []
        for ef in extra_files:
            p = Path(ef).resolve()
            doc_files.append((p, "additional"))

        if not doc_files:
            die("No documents configured. Set scope_files, roadmap_files, or "
                "additional_docs in .researchhub.json, or use --file.")

        # Read file contents and build request
        documents = []
        total_bytes = 0
        missing = []

        for filepath, doc_type in doc_files:
            if not filepath.exists():
                missing.append(str(filepath))
                continue
            try:
                content = filepath.read_text(encoding="utf-8", errors="replace")
            except Exception as e:
                print(f"  Warning: cannot read {filepath}: {e}")
                continue
            if not content.strip():
                print(f"  Warning: {filepath} is empty, skipping")
                continue

            documents.append({
                "filename": filepath.name,
                "content": content,
                "doc_type": doc_type,
            })
            total_bytes += len(content.encode("utf-8"))

        if missing:
            print(f"  Warning: {len(missing)} file(s) not found:")
            for m in missing:
                print(f"    - {m}")

        if not documents:
            die("No readable documents found")

        # POST to docs/ingest
        print(f"Syncing {len(documents)} doc(s) ({total_bytes / 1024:.1f} KB) "
              f"to workspace '{ws}'...")

        data = api_post(url, f"/api/workspaces/{ws}/docs/ingest",
                        {"documents": documents}, timeout=120)

        indexed = data.get("documents_indexed", 0)
        failed = data.get("documents_failed", 0)
        chunks = data.get("total_chunks", 0)
        has_emb = data.get("embeddings_available", False)

        print(f"\nSynced {indexed} doc(s) ({chunks} chunks) to workspace '{ws}'")
        if has_emb:
            print("  Embeddings: generated (semantic search available)")
        else:
            print("  Embeddings: not available (keyword search only)")
        if failed:
            print(f"  Failed: {failed}")

        # Show per-file details
        for d in data.get("details", []):
            status = d.get("status", "?")
            name = d.get("filename", "?")
            ch = d.get("chunks_stored", 0)
            err = d.get("error", "")
            if status == "indexed":
                print(f"  + {name} ({ch} chunks)")
            elif status == "skipped":
                print(f"  ~ {name} (skipped: {err})")
            else:
                print(f"  ! {name} (failed: {err})")

    else:
        die(f"Unknown docs command: {sub}. Use: sync")


# =============================================================================
# Command: export / import
# =============================================================================

def cmd_export(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    if not ws:
        die("Workspace required")

    data = api_get(url, f"/api/workspaces/{ws}/export")
    if args.output:
        with open(args.output, "w") as f:
            json.dump(data, f, indent=2)
        print(f"Exported workspace {ws} to {args.output}")
    else:
        out_json(data)


# =============================================================================
# Command: doi
# =============================================================================

def cmd_doi(args, config):
    url = config["url"]
    sub = args.doi_cmd

    if sub == "resolve":
        if not args.doi:
            die("DOI is required")
        data = api_get(url, f"/api/doi/resolve/{args.doi}")
        out_json(data)

    elif sub == "validate":
        if not args.doi:
            die("DOI is required")
        data = api_get(url, f"/api/doi/validate", params={"doi": args.doi})
        out_json(data)

    elif sub == "references":
        if not args.doi:
            die("DOI is required")
        data = api_get(url, f"/api/doi/references/{args.doi}")
        out_json(data)

    else:
        die(f"Unknown doi command: {sub}")


# =============================================================================
# Command: analyze
# =============================================================================

def cmd_analyze(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    if not ws:
        die("Workspace required")

    print(f"Starting full analysis for workspace: {ws}")
    data = api_post(url, f"/api/workspaces/{ws}/analyze", timeout=600)
    out_json(data)


# =============================================================================
# Command: scope (B32 — workspace scope management)
# =============================================================================

def cmd_scope(args, config):
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    if not ws:
        die("Workspace required")

    sub = args.scope_cmd
    if sub == "get":
        data = api_get(url, f"/api/workspaces/{ws}/scope")
        scope_text = data.get("scope", "")
        if scope_text:
            print(scope_text)
        else:
            print("(no scope set)")
    elif sub == "set":
        text = args.text
        if args.file:
            with open(args.file, "r") as f:
                text = f.read()
        if not text:
            die("Provide scope text via --text or --file")
        data = api_put(url, f"/api/workspaces/{ws}", {"scope": text})
        print(f"Scope updated ({len(data.get('scope', ''))} chars)")
    elif sub == "import":
        file_path = os.path.abspath(args.file_path)
        data = api_post(url, f"/api/workspaces/{ws}/scope/import", {"file_path": file_path})
        print(f"Imported scope from {data.get('source', file_path)} ({len(data.get('scope', ''))} chars)")
    else:
        die("Usage: scope {get|set|import}")


# =============================================================================
# Command: restore (B33 — rebuild workspace from project config)
# =============================================================================

def cmd_restore(args, config):
    """Restore a workspace from .researchhub.json config.

    Reads the project config and calls the restore API to:
    1. Create workspace (if needed)
    2. Import scope from scope_file
    3. Register source folders
    4. Scan source files
    5. Import data from workspace_export.json (if found in autosync folders)
    """
    url = config["url"]
    ws = args.workspace or config.get("workspace")
    if not ws:
        die("Workspace required (set 'workspace' in .researchhub.json or use --workspace)")

    # Build restore payload from config
    payload = {
        "description": f"Restored from {Path.cwd().name} project config",
        "auto_scan": not args.no_scan,
    }

    # Scope files: scope_files (plural) takes precedence over scope_file (singular)
    scope_files = config.get("scope_files", [])
    scope_file = config.get("scope_file")
    if scope_files:
        # Use first found scope file for the restore API (which accepts a single scope_file)
        for sf in scope_files:
            scope_path = Path.cwd() / sf if not os.path.isabs(sf) else Path(sf)
            if scope_path.exists():
                payload["scope_file"] = str(scope_path.resolve())
                print(f"  Scope file: {scope_path}")
                break
        else:
            print(f"  Warning: no scope files found from scope_files: {scope_files}")
    elif scope_file:
        scope_path = Path.cwd() / scope_file if not os.path.isabs(scope_file) else Path(scope_file)
        if scope_path.exists():
            payload["scope_file"] = str(scope_path.resolve())
            print(f"  Scope file: {scope_path}")
        else:
            print(f"  Warning: scope file not found: {scope_path}")

    # Source folders from config
    source_folders = config.get("source_folders", [])
    if source_folders:
        resolved_folders = []
        for folder in source_folders:
            folder_path = folder.get("path", "")
            # Resolve relative paths to absolute
            if not os.path.isabs(folder_path):
                folder_path = str((Path.cwd() / folder_path).resolve())
            resolved_folders.append({
                "path": folder_path,
                "label": folder.get("label", Path(folder_path).name),
                "sync_mode": folder.get("mode", folder.get("sync_mode", "readonly")),
                "file_patterns": folder.get("file_patterns"),
                "recursive": folder.get("recursive", True),
            })
        payload["source_folders"] = resolved_folders
        print(f"  Source folders: {len(resolved_folders)}")

    # Check for workspace_export.json in autosync folders
    for folder in source_folders:
        folder_path = folder.get("path", "")
        if not os.path.isabs(folder_path):
            folder_path = str((Path.cwd() / folder_path).resolve())
        export_path = Path(folder_path) / "workspace_export.json"
        if export_path.exists():
            payload["export_file"] = str(export_path)
            print(f"  Export data: {export_path}")
            break

    print(f"\nRestoring workspace '{ws}'...")
    data = api_post(url, f"/api/workspaces/{ws}/restore", payload, timeout=120)

    # Display results
    print(f"\n  Created:          {'yes (new)' if data.get('created') else 'no (existing)'}")
    print(f"  Scope imported:   {'yes' if data.get('scope_imported') else 'no'}")
    print(f"  Folders added:    {data.get('source_folders_added', 0)}")
    print(f"  Folders skipped:  {data.get('source_folders_skipped', 0)}")
    print(f"  Files scanned:    {data.get('files_scanned', 0)}")
    print(f"  Data imported:    {'yes' if data.get('data_imported') else 'no'}")

    if data.get("import_stats"):
        stats = data["import_stats"]
        print(f"\n  Import details:")
        print(f"    Papers:  {stats.get('papers_added', 0)} added, {stats.get('papers_skipped', 0)} skipped")
        print(f"    Notes:   {stats.get('notes_added', 0)} added")
        print(f"    Links:   {stats.get('links_added', 0)} added")
        print(f"    Queue:   {stats.get('queue_added', 0)} added")

    print(f"\nRestore complete. To ingest files for RAG search:")
    print(f"  python researchhub_client.py source ingest --force")


# =============================================================================
# Command: init (create config file)
# =============================================================================

def cmd_init(args, config):
    """Create a .researchhub.json config file in the current directory."""
    conf = {
        "url": args.url or config["url"],
        "workspace": args.workspace or config.get("workspace"),
    }
    if args.scope_file:
        # If multiple --scope-file flags given, use scope_files (plural)
        if isinstance(args.scope_file, list) and len(args.scope_file) > 1:
            conf["scope_files"] = args.scope_file
        else:
            # Single value: use singular for backward compat
            val = args.scope_file if isinstance(args.scope_file, str) else args.scope_file[0]
            conf["scope_file"] = val
    if args.roadmap_file:
        conf["roadmap_files"] = args.roadmap_file
    if args.additional_doc:
        conf["additional_docs"] = args.additional_doc
    if args.project_root:
        conf["project_root"] = args.project_root
    if args.max_perspectives_per_question is not None:
        conf["max_perspectives_per_question"] = args.max_perspectives_per_question
    if args.source_folder:
        folders = []
        for sf in args.source_folder:
            # Parse "path:label:mode" or just "path"
            parts = sf.split(":", 2)
            folder_entry = {"path": parts[0]}
            if len(parts) > 1:
                folder_entry["label"] = parts[1]
            if len(parts) > 2:
                folder_entry["mode"] = parts[2]
            folders.append(folder_entry)
        conf["source_folders"] = folders
    if args.auto_sort_uploads:
        conf["auto_sort_uploads"] = True
    if args.auto_git_cleanup:
        conf["auto_git_cleanup"] = True
        conf["git_cleanup_mode"] = args.git_cleanup_mode or "managed"
        conf["git_cleanup_compress"] = not args.no_git_cleanup_compress
    elif args.git_cleanup_mode is not None:
        # Mode specified without --auto-git-cleanup: store mode but don't enable
        conf["git_cleanup_mode"] = args.git_cleanup_mode
    config_path = Path.cwd() / CONFIG_FILE
    if config_path.exists() and not args.force:
        die(f"{CONFIG_FILE} already exists. Use --force to overwrite.")
    with open(config_path, "w") as f:
        json.dump(conf, f, indent=2)
    print(f"Created {config_path}")
    print(json.dumps(conf, indent=2))


# =============================================================================
# Command: git-cleanup (preview / status / execute for PDF history cleanup)
# =============================================================================

def _is_git_repo(path):
    """Check if a path is a git repository."""
    return (Path(path) / ".git").is_dir()


def _git_size(path):
    """Return .git directory size in bytes, or 0 if not a git repo."""
    git_dir = Path(path) / ".git"
    if not git_dir.is_dir():
        return 0
    try:
        result = subprocess.run(
            ["du", "-sb", str(git_dir)], capture_output=True, text=True, timeout=30
        )
        return int(result.stdout.split()[0]) if result.returncode == 0 else 0
    except (subprocess.TimeoutExpired, ValueError, IndexError):
        return 0


def _human_size(nbytes):
    """Format byte count as human-readable string."""
    if nbytes < 1024:
        return f"{nbytes} B"
    elif nbytes < 1024 * 1024:
        return f"{nbytes / 1024:.1f} KB"
    elif nbytes < 1024 * 1024 * 1024:
        return f"{nbytes / (1024 * 1024):.1f} MB"
    else:
        return f"{nbytes / (1024 * 1024 * 1024):.1f} GB"


def _count_pdfs(path):
    """Count PDF files in a directory (excluding .git)."""
    count = 0
    for root, dirs, files in os.walk(path):
        dirs[:] = [d for d in dirs if d != ".git"]
        for f in files:
            if f.lower().endswith(".pdf"):
                count += 1
    return count


def _count_pdfs_in_history(path):
    """Count unique PDFs that have ever been in git history."""
    try:
        result = subprocess.run(
            ["git", "-C", str(path), "log", "--all", "--diff-filter=A",
             "--name-only", "--", "*.pdf"],
            capture_output=True, text=True, timeout=60
        )
        if result.returncode != 0:
            return 0
        pdfs = set()
        for line in result.stdout.strip().splitlines():
            if line.lower().endswith(".pdf"):
                pdfs.add(line)
        return len(pdfs)
    except (subprocess.TimeoutExpired, OSError):
        return 0


def _local_cleanup_preview(path):
    """Run a local analysis of a single repo for cleanup preview."""
    path = Path(path).resolve()
    if not _is_git_repo(path):
        return {"path": str(path), "error": "Not a git repository"}

    git_bytes = _git_size(path)
    pdf_count = _count_pdfs(path)
    pdfs_in_history = _count_pdfs_in_history(path)

    # Estimate bloat: if PDFs are in history, .git is likely inflated
    bloat_estimate = "none"
    if pdfs_in_history > 0:
        if git_bytes > 100 * 1024 * 1024:
            bloat_estimate = "high"
        elif git_bytes > 20 * 1024 * 1024:
            bloat_estimate = "moderate"
        else:
            bloat_estimate = "low"

    return {
        "path": str(path),
        "name": path.name,
        "is_git_repo": True,
        "git_size_bytes": git_bytes,
        "git_size_human": _human_size(git_bytes),
        "pdfs_on_disk": pdf_count,
        "pdfs_in_history": pdfs_in_history,
        "bloat_estimate": bloat_estimate,
    }


def _find_repos_from_config(config):
    """Find git repos from .researchhub.json source_folders."""
    repos = []

    # Read config file for source_folders
    config_path = Path.cwd() / CONFIG_FILE
    if not config_path.exists():
        return repos

    try:
        with open(config_path) as f:
            file_config = json.load(f)
    except (json.JSONDecodeError, OSError):
        return repos

    source_folders = file_config.get("source_folders", [])
    seen = set()
    for folder in source_folders:
        folder_path = folder.get("path", "")
        if not os.path.isabs(folder_path):
            folder_path = str((Path.cwd() / folder_path).resolve())

        # Walk up to find the git root for this source folder
        check = Path(folder_path)
        while check != check.parent:
            if (check / ".git").is_dir() and str(check) not in seen:
                seen.add(str(check))
                repos.append(str(check))
                break
            check = check.parent

    # Also check cwd itself
    if _is_git_repo(Path.cwd()) and str(Path.cwd().resolve()) not in seen:
        repos.insert(0, str(Path.cwd().resolve()))

    return repos


def cmd_backup(args, config):
    """Trigger on-demand backup of workspace databases."""
    url = config["url"]
    sub = args.backup_cmd

    if sub == "trigger":
        ws = args.backup_workspace or config.get("workspace")
        if ws:
            # Single workspace backup
            print(f"Triggering backup for workspace '{ws}'...")
            data = api_post(url, f"/api/workspaces/{ws}/backup", timeout=120)
            print(f"Backup complete:")
            out_status("Workspace", data.get("workspace"))
            out_status("Size", f"{data.get('size_mb', 0)} MB ({data.get('size_bytes', 0)} bytes)")
            out_status("Duration", f"{data.get('duration_seconds', 0)}s")
            out_status("Path", data.get("backup_path"))
        else:
            # All workspaces
            print("Triggering backup for all workspaces...")
            data = api_post(url, "/api/backup/trigger", timeout=300)
            print(f"Backup complete:")
            out_status("Workspaces backed up", data.get("workspaces_backed_up", 0))
            out_status("Workspaces skipped", data.get("workspaces_skipped", 0))
            out_status("Total size", f"{data.get('total_size_mb', 0)} MB")
            if data.get("errors"):
                print(f"  Errors ({len(data['errors'])}):")
                for err in data["errors"]:
                    print(f"    - {err}")
            if data.get("repo_sync"):
                sync = data["repo_sync"]
                if "error" in sync:
                    out_status("Repo sync", f"failed: {sync['error']}")
                else:
                    out_status("Repo sync", "triggered")

    elif sub == "status":
        data = api_get(url, "/api/backup/status")
        out_status("Enabled", data.get("enabled"))
        out_status("Running", data.get("running"))
        out_status("Last backup", data.get("last_backup_at", "never"))
        out_status("Total cycles", data.get("total_cycles", 0))
        out_status("Workspaces backed up", data.get("workspaces_backed_up", 0))
        total_kb = data.get("total_disk_bytes", 0) / 1024
        out_status("Total disk usage", f"{total_kb:.1f} KB")
        if data.get("last_error"):
            out_status("Last error", data["last_error"])

    else:
        print("Usage: researchhub_client.py backup <trigger|status> [--workspace NAME]")
        sys.exit(1)


def cmd_git_cleanup(args, config):
    url = config["url"]
    sub = args.git_cleanup_cmd

    if sub == "preview":
        target_path = getattr(args, "path", None)

        # Try server first
        server_available = False
        try:
            api_get(url, "/api/diagnostics/health", timeout=5)
            server_available = True
        except (APIError, Exception):
            pass

        if server_available:
            try:
                params = {}
                if target_path:
                    params["path"] = os.path.abspath(target_path)
                data = api_get(url, "/api/repos/git-cleanup/preview", params=params, timeout=60)
                out_json(data)
                return
            except APIError as e:
                if e.status == 404:
                    print("Server endpoint not available, falling back to local analysis...")
                else:
                    print(f"Server error (HTTP {e.status}), falling back to local analysis...")

        # Local fallback
        if target_path:
            paths = [os.path.abspath(target_path)]
        else:
            paths = _find_repos_from_config(config)
            if not paths:
                # Last resort: check current directory
                if _is_git_repo(Path.cwd()):
                    paths = [str(Path.cwd().resolve())]
                else:
                    die("No git repos found. Use --path or run from a project with .researchhub.json")

        print(f"Analyzing {len(paths)} repo(s) for PDF history bloat...\n")

        results = []
        for p in paths:
            info = _local_cleanup_preview(p)
            results.append(info)

        # Display as table
        rows = []
        for r in results:
            if r.get("error"):
                rows.append([r.get("path", "?"), "ERROR", "-", "-", "-", r["error"]])
            else:
                rows.append([
                    r.get("name", "?"),
                    r.get("git_size_human", "?"),
                    str(r.get("pdfs_on_disk", 0)),
                    str(r.get("pdfs_in_history", 0)),
                    r.get("bloat_estimate", "?"),
                    r.get("path", "?"),
                ])
        out_table(rows, ["Repo", ".git Size", "PDFs Disk", "PDFs History", "Bloat", "Path"])

        # Summary
        total_git = sum(r.get("git_size_bytes", 0) for r in results if not r.get("error"))
        total_history = sum(r.get("pdfs_in_history", 0) for r in results if not r.get("error"))
        print(f"\nTotal .git size: {_human_size(total_git)}")
        print(f"Total PDFs in history: {total_history}")
        if total_history > 0:
            print(f"\nTo clean a specific repo:")
            print(f"  scripts/cleanup_pdf_history.sh /path/to/repo --dry-run")

    elif sub == "status":
        ws = args.workspace or config.get("workspace")

        # Try server first
        server_available = False
        try:
            api_get(url, "/api/diagnostics/health", timeout=5)
            server_available = True
        except (APIError, Exception):
            pass

        if server_available and ws:
            try:
                data = api_get(url, f"/api/workspaces/{ws}/source-repos", timeout=30)
                repos = data if isinstance(data, list) else data.get("repos", [])
                if repos:
                    rows = []
                    for r in repos:
                        rows.append([
                            r.get("label", r.get("name", "?")),
                            r.get("path", "?"),
                            "git" if r.get("is_git_repo") else "dir",
                            str(r.get("pdf_count", "-")),
                            r.get("git_size_human", "-"),
                        ])
                    out_table(rows, ["Label", "Path", "Type", "PDFs", ".git Size"])
                    return
                else:
                    print("No source repos found via server.")
                    return
            except APIError as e:
                if e.status == 404:
                    print("Server endpoint not available, falling back to local analysis...")
                else:
                    print(f"Server error (HTTP {e.status}), falling back to local analysis...")

        # Local fallback: examine source_folders from config
        config_path = Path.cwd() / CONFIG_FILE
        if not config_path.exists():
            die(f"No {CONFIG_FILE} found. Run from a project directory or use --workspace.")

        try:
            with open(config_path) as f:
                file_config = json.load(f)
        except (json.JSONDecodeError, OSError) as exc:
            die(f"Cannot read {CONFIG_FILE}: {exc}")

        source_folders = file_config.get("source_folders", [])
        if not source_folders:
            print("No source folders in config.")
            return

        rows = []
        for folder in source_folders:
            folder_path = folder.get("path", "")
            label = folder.get("label", folder_path)
            if not os.path.isabs(folder_path):
                folder_path = str((Path.cwd() / folder_path).resolve())

            is_git = _is_git_repo(folder_path)
            folder_type = "git" if is_git else "dir"
            pdf_count = _count_pdfs(folder_path) if os.path.isdir(folder_path) else 0
            git_size = _human_size(_git_size(folder_path)) if is_git else "-"

            rows.append([label, folder_path, folder_type, str(pdf_count), git_size])

        out_table(rows, ["Label", "Path", "Type", "PDFs", ".git Size"])

    elif sub == "gc":
        target_path = getattr(args, "path", None)
        run_all = getattr(args, "repos", False)

        # Standalone mode: run git gc directly on a local path
        if target_path:
            target = Path(os.path.abspath(target_path))
            if not _is_git_repo(target):
                die(f"Not a git repository: {target}")

            before = _git_size(target)
            print(f"Running git gc --prune=now on {target}...")
            print(f"  Before: {_human_size(before)}")

            try:
                result = subprocess.run(
                    ["git", "-C", str(target), "gc", "--prune=now"],
                    capture_output=True, text=True, timeout=300,
                )
                if result.returncode != 0:
                    die(f"git gc failed: {result.stderr.strip()}")
            except subprocess.TimeoutExpired:
                die("git gc timed out (>300s)")

            after = _git_size(target)
            savings = before - after
            print(f"  After:  {_human_size(after)}")
            print(f"  Saved:  {_human_size(max(0, savings))}")
            return

        # API mode: run gc via server on configured repos
        server_available = False
        try:
            api_get(url, "/api/diagnostics/health", timeout=5)
            server_available = True
        except (APIError, Exception):
            pass

        if server_available and run_all:
            # Run gc on all configured repos via API
            try:
                preview_data = api_get(url, "/api/repos/git-cleanup/preview", timeout=60)
                repos = preview_data.get("repositories", [])
                if not repos:
                    print("No configured repositories found.")
                    return

                rows = []
                for repo in repos:
                    name = repo.get("name", "?")
                    print(f"Running gc on {name}...")
                    try:
                        gc_result = api_post(url, f"/api/repos/{name}/git-cleanup/gc", timeout=300)
                        rows.append([
                            name,
                            f"{gc_result.get('before_size_mb', 0):.1f} MB",
                            f"{gc_result.get('after_size_mb', 0):.1f} MB",
                            f"{gc_result.get('savings_mb', 0):.1f} MB",
                            "OK" if gc_result.get("success") else gc_result.get("error", "failed"),
                        ])
                    except APIError as e:
                        rows.append([name, "-", "-", "-", f"Error: {e}"])

                print()
                out_table(rows, ["Repo", "Before", "After", "Saved", "Status"])
                return
            except APIError as e:
                print(f"Server error: {e}. Falling back to local mode...")

        # Local fallback: find repos from config and gc each
        if run_all:
            paths = _find_repos_from_config(config)
            if not paths:
                die("No git repos found. Use --path or run from a project with .researchhub.json")
        else:
            # Single repo: current directory
            cwd = str(Path.cwd().resolve())
            if not _is_git_repo(cwd):
                die("Not in a git repository. Use --path PATH or --repos to gc all configured repos.")
            paths = [cwd]

        rows = []
        for p in paths:
            name = Path(p).name
            before = _git_size(p)
            print(f"Running git gc --prune=now on {name}...")
            try:
                result = subprocess.run(
                    ["git", "-C", p, "gc", "--prune=now"],
                    capture_output=True, text=True, timeout=300,
                )
                after = _git_size(p)
                savings = before - after
                status = "OK" if result.returncode == 0 else f"rc={result.returncode}"
                rows.append([name, _human_size(before), _human_size(after), _human_size(max(0, savings)), status])
            except subprocess.TimeoutExpired:
                rows.append([name, _human_size(before), "-", "-", "timeout"])

        print()
        out_table(rows, ["Repo", "Before", "After", "Saved", "Status"])

    elif sub == "execute":
        print("Execution mode not yet available. Use preview to see what would be cleaned.")
        print("")
        print("To run cleanup manually:")
        path_flag = getattr(args, "path", None)
        if path_flag:
            print(f"  scripts/cleanup_pdf_history.sh {os.path.abspath(path_flag)} --dry-run")
        else:
            print("  scripts/cleanup_pdf_history.sh /path/to/repo --dry-run")
        print("")
        print("Options:")
        print("  --compress      Compress PDFs with pikepdf before cleaning")
        print("  --force-push    Force push to remote after cleanup")
        print("  --dry-run       Audit only, make no changes")

    else:
        die(f"Unknown git-cleanup command: {sub}")


# =============================================================================
# Argument Parser
# =============================================================================

def build_parser():
    parser = argparse.ArgumentParser(
        prog="researchhub_client.py",
        description="ResearchHub API Client — drop-in for any project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s health
  %(prog)s workspace list
  %(prog)s -w my-topic paper list
  %(prog)s -w my-topic paper enrich
  %(prog)s -w my-topic cite export --format endnote -o refs.xml
  %(prog)s -w my-topic note generate --source papers
  %(prog)s research submit --topic "machine learning"
  %(prog)s -w my-topic upload file myfile.pdf
  %(prog)s -w my-topic upload file myfile.pdf --subfolder sources/pdfs
  %(prog)s -w my-topic upload sort --dry-run
  %(prog)s --url http://localhost:5347 -w my-topic init --auto-sort-uploads
  %(prog)s git-cleanup preview
  %(prog)s git-cleanup preview --path /home/devel/myrepo
  %(prog)s git-cleanup status

Documentation:
  See ~/researchhub/docs/wiki/ for guides:
    WORKSPACE_FILES.md   — File structure and where to put notes
    PROJECT_SETUP.md     — Setting up a new project
    RESEARCH_WORKFLOW.md — Research pipeline from questions to answers
""",
    )
    parser.add_argument("--url", help="ResearchHub URL (default: from config)")
    parser.add_argument("--workspace", "-w", help="Default workspace name")
    parser.add_argument("--json", action="store_true", help="Force JSON output")

    sub = parser.add_subparsers(dest="command", help="Command group")

    # health
    sub.add_parser("health", help="Check ResearchHub health")

    # init
    p_init = sub.add_parser("init", help="Create .researchhub.json config")
    p_init.add_argument("--force", action="store_true", help="Overwrite existing config")
    p_init.add_argument("--scope-file", action="append", help="Path to project scope file. Repeatable; multiple values use scope_files (plural).")
    p_init.add_argument("--roadmap-file", action="append", help="Path to roadmap file for research context. Repeatable.")
    p_init.add_argument("--additional-doc", action="append", help="Extra documentation file for question generation context. Repeatable.")
    p_init.add_argument("--project-root", default=None, help="Root directory for resolving relative doc paths (default: '.')")
    p_init.add_argument("--max-perspectives-per-question", type=int, default=None, help="Max perspectives per generated question (default: 3)")
    p_init.add_argument("--source-folder", action="append", help="Source folder (path:label:mode). Repeatable.")
    p_init.add_argument("--auto-sort-uploads", action="store_true", help="Enable automatic sorting of uploads/ staging files")
    p_init.add_argument("--auto-git-cleanup", action="store_true", help="Enable automatic PDF history cleanup in git repo")
    p_init.add_argument("--git-cleanup-mode", choices=["managed", "initial"], default=None,
                        help="Cleanup mode: 'managed' (ResearchHub folders only) or 'initial' (all PDFs)")
    p_init.add_argument("--no-git-cleanup-compress", action="store_true",
                        help="Disable PDF compression before cleanup (compression enabled by default)")

    # workspace
    p_ws = sub.add_parser("workspace", help="Workspace operations")
    ws_sub = p_ws.add_subparsers(dest="workspace_cmd")
    ws_sub.add_parser("list", help="List all workspaces")
    p_ws_create = ws_sub.add_parser("create", help="Create a workspace")
    p_ws_create.add_argument("--topic", required=True)
    p_ws_create.add_argument("--description", default="")
    p_ws_detail = ws_sub.add_parser("detail", help="Get workspace details")
    p_ws_detail.add_argument("--name", help="Workspace name")
    p_ws_state = ws_sub.add_parser("state", help="Get/set workspace state")
    p_ws_state.add_argument("--name", help="Workspace name")
    p_ws_state.add_argument("--set", dest="set_state", help="Set state (UPLOAD/ANALYSIS/DOCUMENTATION/RESEARCH)")
    p_ws_stats = ws_sub.add_parser("stats", help="Get workspace statistics")
    p_ws_stats.add_argument("--name", help="Workspace name")

    # workspace set-type / get-type (B97)
    p_ws_set_type = ws_sub.add_parser("set-type", help="Set workspace type (research or test)")
    p_ws_set_type.add_argument("--name", help="Workspace name")
    p_ws_set_type.add_argument("--type", dest="type_value", choices=["research", "test"], help="Workspace type")
    p_ws_get_type = ws_sub.add_parser("get-type", help="Get workspace type")
    p_ws_get_type.add_argument("--name", help="Workspace name")

    # workspace fts-rebuild (B58b)
    p_ws_fts = ws_sub.add_parser("fts-rebuild", help="Rebuild FTS5 search indexes (B58b)")
    p_ws_fts.add_argument("--name", help="Workspace name")

    # workspace absorb (B50b)
    p_ws_absorb = ws_sub.add_parser("absorb", help="Absorb content from source workspace into target")
    p_ws_absorb.add_argument("--source", required=True, help="Source workspace name to absorb from")
    p_ws_absorb.add_argument("--target", help="Target workspace name (default: current workspace)")
    p_ws_absorb.add_argument("--no-papers", action="store_true", help="Skip paper import")
    p_ws_absorb.add_argument("--no-notes", action="store_true", help="Skip note import")
    p_ws_absorb.add_argument("--no-pdfs", action="store_true", help="Skip PDF copy")
    p_ws_absorb.add_argument("--no-generated", action="store_true", help="Skip generated docs")
    p_ws_absorb.add_argument("--include-queue", action="store_true", help="Also absorb research queue")
    p_ws_absorb.add_argument("--include-runs", action="store_true", help="Also absorb research runs")
    p_ws_absorb.add_argument("--dry-run", action="store_true", help="Preview what would be absorbed")
    p_ws_absorb.add_argument("--delete-source", action="store_true", help="Delete source workspace after absorb")

    # workspace reset
    p_ws_reset = ws_sub.add_parser("reset", help="Reset workspace database (clear research state, keep structure)")
    p_ws_reset.add_argument("--name", help="Workspace name")
    p_ws_reset.add_argument("--keep-papers", action="store_true", default=True, help="Keep papers table (default)")
    p_ws_reset.add_argument("--no-keep-papers", action="store_true", help="Also clear papers table")
    p_ws_reset.add_argument("--keep-notes", action="store_true", default=False, help="Keep atomic notes")
    p_ws_reset.add_argument("--delete-pdfs", action="store_true", default=False, help="Delete PDF files from disk")
    p_ws_reset.add_argument("--trigger-ingest", action="store_true", default=False, help="Re-trigger ingestion after reset")

    # paper
    p_paper = sub.add_parser("paper", help="Paper registry operations")
    pa_sub = p_paper.add_subparsers(dest="paper_cmd")
    pa_sub.add_parser("list", help="List papers in workspace")
    p_pa_add = pa_sub.add_parser("add", help="Add a paper")
    p_pa_add.add_argument("--title", required=True)
    p_pa_add.add_argument("--authors", help="Comma-separated authors")
    p_pa_add.add_argument("--year", type=int)
    p_pa_add.add_argument("--doi")
    p_pa_enrich = pa_sub.add_parser("enrich", help="Enrich papers with citation data")
    p_pa_enrich.add_argument("--force", action="store_true", help="Re-enrich all")
    # B65b: REC-13 — paper search subcommand
    p_pa_search = pa_sub.add_parser("search", help="Search papers by title/content")
    p_pa_search.add_argument("--query", "-q", required=True, help="Search query")
    p_pa_search.add_argument("--limit", type=int, default=20, help="Max results")
    p_pa_audit = pa_sub.add_parser("audit", help="Audit papers against workspace questions (paper police)")
    p_pa_audit.add_argument("--all", dest="all_workspaces", action="store_true",
                            help="Audit all workspaces (no --workspace needed)")
    p_pa_audit.add_argument("--detail", action="store_true",
                            help="Include full detail in global audit (default: summary only)")
    p_pa_cleanup = pa_sub.add_parser("cleanup", help="Preview/delete orphaned papers")
    p_pa_cleanup.add_argument("--dry-run", action="store_true", default=True,
                              help="Preview only (default)")
    p_pa_cleanup.add_argument("--execute", action="store_true",
                              help="Actually delete orphaned papers")
    p_pa_cleanup.add_argument("--all", dest="all_workspaces", action="store_true",
                              help="Cleanup across all workspaces")

    # note
    p_note = sub.add_parser("note", help="Atomic notes operations")
    no_sub = p_note.add_subparsers(dest="note_cmd")
    p_no_list = no_sub.add_parser("list", help="List notes")
    p_no_list.add_argument("--type", help="Filter by type: concept, evidence, hub, literature, method, question")
    p_no_gen = no_sub.add_parser("generate", help="Generate notes from papers or documents")
    p_no_gen.add_argument("--source", choices=["papers", "documents"], default="papers")
    p_no_gen.add_argument("--max-sources", type=int)
    p_no_status = no_sub.add_parser("status", help="Check note generation job status")
    p_no_status.add_argument("--job-id", required=True)
    p_no_search = no_sub.add_parser("search", help="Search notes")
    p_no_search.add_argument("--query", "-q", required=True)
    no_sub.add_parser("relink", help="Re-run auto-linking on all notes (B68e)")

    # cite
    p_cite = sub.add_parser("cite", help="Citation export")
    ci_sub = p_cite.add_subparsers(dest="cite_cmd")
    p_ci_export = ci_sub.add_parser("export", help="Export citations")
    p_ci_export.add_argument("--format", "-f", choices=["bibtex", "ris", "endnote"], default="bibtex")
    p_ci_export.add_argument("--output", "-o", help="Output file path")
    ci_sub.add_parser("stats", help="Citation statistics")

    # source
    p_src = sub.add_parser("source", help="Source folder operations")
    sr_sub = p_src.add_subparsers(dest="source_cmd")
    sr_sub.add_parser("list", help="List source folders")
    p_sr_add = sr_sub.add_parser("add", help="Add a source folder")
    p_sr_add.add_argument("--path", required=True, help="Filesystem path")
    p_sr_add.add_argument("--label", help="Display label")
    p_sr_add.add_argument("--pattern", help="File glob pattern (e.g. *.pdf)")
    p_sr_add.add_argument("--mode", choices=["readonly", "autosync"], default="readonly")
    sr_sub.add_parser("scan", help="Scan all source folders")
    p_sr_ingest = sr_sub.add_parser("ingest", help="Ingest source files for RAG")
    p_sr_ingest.add_argument("--max-files", type=int)
    p_sr_ingest.add_argument("--force", action="store_true")
    sr_sub.add_parser("sync", help="Sync autosync folders")

    # research
    p_res = sub.add_parser("research", help="Research execution (B34)")
    re_sub = p_res.add_subparsers(dest="research_cmd")

    # research execute (B34)
    p_re_exec = re_sub.add_parser("execute", help="Execute batch research on queue topics")
    p_re_exec.add_argument("--mode", required=True, choices=["web", "academic", "deep"])
    p_re_exec.add_argument("--max", type=int, help="Max topics to run")
    p_re_exec.add_argument("--priority", type=int, help="Filter by priority level")
    p_re_exec.add_argument("--depth", choices=["quick", "standard", "comprehensive"], default="standard")
    p_re_exec.add_argument("--topics", help="Comma-separated topic IDs")
    p_re_exec.add_argument("--force", action="store_true", help="Re-run even if mode completed")

    # research status (B34 execution_id or legacy job-id)
    # B53f: Support both positional and --execution-id flag
    p_re_status = re_sub.add_parser("status", help="Check execution status")
    p_re_status.add_argument("id", nargs="?", default=None, help="Execution ID (positional)")
    p_re_status.add_argument("--execution-id", dest="execution_id_flag", help="B34 execution ID (flag)")
    p_re_status.add_argument("--job-id", help="Legacy integration job ID")
    p_re_status.add_argument("--list", action="store_true", help="List recent runs")

    # research cancel/pause/resume (B34)
    p_re_cancel = re_sub.add_parser("cancel", help="Cancel a research run")
    p_re_cancel.add_argument("--run-id", required=True)
    p_re_pause = re_sub.add_parser("pause", help="Pause a research run")
    p_re_pause.add_argument("--run-id", required=True)
    p_re_resume = re_sub.add_parser("resume", help="Resume a paused run")
    p_re_resume.add_argument("--run-id", required=True)

    # research active (B34)
    re_sub.add_parser("active", help="Show active/paused/queued runs")

    # research runs (B34 topic history)
    p_re_runs = re_sub.add_parser("runs", help="Show run history for a topic")
    p_re_runs.add_argument("--topic-id", required=True)

    # research queue (B49d)
    p_re_queue = re_sub.add_parser("queue", help="Manage research queue")
    p_re_queue.add_argument("action", choices=["list", "add", "remove", "set-priority", "stats"], help="Queue action")
    p_re_queue.add_argument("--topic", help="Topic text (for add)")
    p_re_queue.add_argument("--priority", type=int, default=2, help="Priority 1-10 (for add / set-priority)")
    p_re_queue.add_argument("--topic-id", help="Topic ID (for remove / set-priority)")
    p_re_queue.add_argument("--level", help="Named priority: critical, high, medium, low, backlog (for set-priority)")

    # research query — cross-workspace RAG with RRF scoring
    p_re_query = re_sub.add_parser("query", help="RAG query across workspaces (global RRF)")
    p_re_query.add_argument("question", help="Query string to search")
    p_re_query.add_argument("--workspace", dest="query_workspace", help="Limit to specific workspace")
    p_re_query.add_argument("--threshold", type=float, default=0.5, help="Similarity threshold (default: 0.5)")
    p_re_query.add_argument("--top-k", type=int, default=5, help="Number of results (default: 5)")

    # research questions (B57/B58)
    p_re_questions = re_sub.add_parser("questions", help="Detect open questions in workspace docs")
    p_re_questions.add_argument("--file", help="Specific file path to scan (default: all generated docs)")

    # research provenance (B67c)
    p_re_prov = re_sub.add_parser("provenance", help="Show paper provenance/discovery history for a topic")
    p_re_prov.add_argument("--topic-id", required=True, help="Queue topic ID")

    # research overview (system-wide status, no workspace required)
    p_re_overview = re_sub.add_parser("overview", help="Show system-wide research status (what is ResearchHub doing now?)")
    p_re_overview.add_argument("--json", action="store_true", help="Also output raw JSON")

    # research metrics (cross-workspace, no workspace required)
    re_sub.add_parser("metrics", help="Show cross-workspace research metrics (papers, notes, topics, runs)")

    # research complexity (cross-workspace complexity growth, no workspace required)
    p_re_complexity = re_sub.add_parser("complexity", help="Show research complexity growth (topic discovery vs completion rates)")
    p_re_complexity.add_argument("--json", action="store_true", help="Also output raw JSON")

    # research completeness (dual-completeness: answer vs academic, no workspace required)
    p_re_completeness = re_sub.add_parser("completeness", help="Show dual-completeness: answer vs academic backing quality")
    p_re_completeness.add_argument("--json", action="store_true", help="Also output raw JSON")

    # research evidence (evidence chain for questions)
    p_re_evidence = re_sub.add_parser("evidence", help="Show evidence chain: question -> references -> notes -> answer")
    p_re_evidence.add_argument("--question-id", help="Specific question ID (shows one chain)")
    p_re_evidence.add_argument("--all", action="store_true", dest="show_all", help="Show all questions with evidence chains")
    p_re_evidence.add_argument("--status", choices=["answered", "unanswered", "partial"], help="Filter by status")
    p_re_evidence.add_argument("--json", action="store_true", help="Also output raw JSON")

    # Legacy commands
    p_re_submit = re_sub.add_parser("submit", help="Submit via integration API (legacy)")
    p_re_submit.add_argument("--topic", required=True)
    p_re_submit.add_argument("--depth", choices=["shallow", "normal", "deep"], default="normal")
    p_re_results = re_sub.add_parser("results", help="Get integration results (legacy)")
    p_re_results.add_argument("--job-id", required=True)

    # upload (B45c: subcommand group — file + sort)
    p_upload = sub.add_parser("upload", help="Upload files and manage uploads/ staging")
    up_sub = p_upload.add_subparsers(dest="upload_cmd")
    p_up_file = up_sub.add_parser("file", help="Upload a file to workspace")
    p_up_file.add_argument("file", help="Path to file")
    p_up_file.add_argument("--subfolder", help="Destination subfolder (default: uploads/ staging)")
    p_up_sort = up_sub.add_parser("sort", help="Sort uploads/ staging into categorized folders")
    p_up_sort.add_argument("--dry-run", action="store_true", help="Preview without moving files")

    # docs
    p_docs = sub.add_parser("docs", help="Project documentation sync to workspace RAG")
    docs_sub = p_docs.add_subparsers(dest="docs_cmd")
    p_docs_sync = docs_sub.add_parser("sync", help="Sync project docs (scope, roadmap, etc.) into RAG")
    p_docs_sync.add_argument("--file", action="append", help="Extra file to sync (repeatable)")

    # export
    p_export = sub.add_parser("export", help="Export workspace data as JSON")
    p_export.add_argument("--output", "-o", help="Output file path")

    # doi
    p_doi = sub.add_parser("doi", help="DOI resolution")
    doi_sub = p_doi.add_subparsers(dest="doi_cmd")
    p_doi_res = doi_sub.add_parser("resolve", help="Resolve a DOI")
    p_doi_res.add_argument("--doi", required=True)
    p_doi_val = doi_sub.add_parser("validate", help="Validate a DOI")
    p_doi_val.add_argument("--doi", required=True)
    p_doi_ref = doi_sub.add_parser("references", help="Get DOI references")
    p_doi_ref.add_argument("--doi", required=True)

    # analyze
    sub.add_parser("analyze", help="Run full workspace analysis")

    # scope (B32)
    p_scope = sub.add_parser("scope", help="Workspace scope management (B32)")
    sc_sub = p_scope.add_subparsers(dest="scope_cmd")
    sc_sub.add_parser("get", help="Show workspace scope")
    p_sc_set = sc_sub.add_parser("set", help="Set workspace scope from text or file")
    p_sc_set.add_argument("--text", "-t", help="Scope text")
    p_sc_set.add_argument("--file", "-f", help="Read scope from file")
    p_sc_import = sc_sub.add_parser("import", help="Import scope from project file path")
    p_sc_import.add_argument("file_path", help="Path to scope file (will be resolved to absolute)")

    # restore (B33)
    p_restore = sub.add_parser("restore", help="Restore workspace from .researchhub.json (B33)")
    p_restore.add_argument("--no-scan", action="store_true", help="Skip auto-scan after adding source folders")

    # backup
    p_backup = sub.add_parser("backup", help="Trigger on-demand workspace backups")
    bk_sub = p_backup.add_subparsers(dest="backup_cmd")
    p_bk_trigger = bk_sub.add_parser("trigger", help="Trigger immediate backup")
    p_bk_trigger.add_argument("--workspace", dest="backup_workspace", help="Backup a specific workspace (default: all)")
    bk_sub.add_parser("status", help="Show backup monitor status")

    # git-cleanup (PDF history cleanup preview/status/execute)
    p_gc = sub.add_parser("git-cleanup", help="Git repository PDF history cleanup")
    gc_sub = p_gc.add_subparsers(dest="git_cleanup_cmd")
    p_gc_preview = gc_sub.add_parser("preview", help="Preview cleanup for repos (PDF bloat analysis)")
    p_gc_preview.add_argument("--path", help="Specific repo path (default: all repos in .researchhub.json)")
    p_gc_status = gc_sub.add_parser("status", help="Show source folder git/dir status and PDF counts")
    p_gc_gc = gc_sub.add_parser("gc", help="Run git gc --prune=now to reclaim space from loose objects")
    p_gc_gc.add_argument("--path", help="Specific repo path (runs locally, no server needed)")
    p_gc_gc.add_argument("--repos", action="store_true", help="Run gc on all configured repos")
    p_gc_execute = gc_sub.add_parser("execute", help="Execute cleanup (not yet implemented)")
    p_gc_execute.add_argument("--path", help="Specific repo path")
    p_gc_execute.add_argument("--compress", action="store_true", help="Compress PDFs before cleanup")
    p_gc_execute.add_argument("--force-push", action="store_true", help="Force push to remote after cleanup")

    return parser


# =============================================================================
# Main
# =============================================================================

COMMAND_MAP = {
    "health": cmd_health,
    "init": cmd_init,
    "workspace": cmd_workspace,
    "paper": cmd_paper,
    "note": cmd_note,
    "cite": cmd_cite,
    "source": cmd_source,
    "research": cmd_research,
    "upload": cmd_upload,
    "docs": cmd_docs,
    "export": cmd_export,
    "doi": cmd_doi,
    "analyze": cmd_analyze,
    "scope": cmd_scope,
    "restore": cmd_restore,
    "backup": cmd_backup,
    "git-cleanup": cmd_git_cleanup,
}


def main():
    parser = build_parser()
    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(0)

    # Load config and apply CLI overrides
    config = load_config()
    if args.url:
        config["url"] = args.url
    if hasattr(args, "workspace") and args.workspace:
        config["workspace"] = args.workspace

    # Dispatch
    handler = COMMAND_MAP.get(args.command)
    if not handler:
        die(f"Unknown command: {args.command}")

    try:
        handler(args, config)
    except APIError as e:
        detail = ""
        if e.body:
            try:
                detail = json.loads(e.body).get("detail", e.body[:200])
            except (json.JSONDecodeError, AttributeError):
                detail = e.body[:200]
        die(f"{e.message}" + (f" — {detail}" if detail else ""))
    except KeyboardInterrupt:
        print("\nInterrupted.")
        sys.exit(130)


if __name__ == "__main__":
    main()
