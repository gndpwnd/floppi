"""
Report generation for complexity analysis.

Formats CPU timing, memory usage, and source scan results.
"""

from .platforms import PLATFORMS, PLATFORM_BUILDS, get_cycles


def _bar(pct, width=20):
    """Generate ASCII progress bar."""
    filled = int(min(pct, 100) / 100 * width)
    return "[" + "=" * filled + " " * (width - filled) + "]"


def _format_bytes(n):
    """Format byte count as human-readable."""
    if n >= 1024 * 1024:
        return f"{n / 1024 / 1024:.1f} MB"
    if n >= 1024:
        return f"{n / 1024:.1f} KB"
    return f"{n} B"


def _status(pct):
    """Status indicator based on usage percentage."""
    if pct < 50:
        return "OK"
    if pct < 75:
        return "MODERATE"
    if pct < 90:
        return "HIGH"
    return "CRITICAL"


def _worst_status(*pcts):
    """Return worst status from multiple percentages (ignoring None)."""
    valid = [p for p in pcts if p is not None]
    if not valid:
        return "OK"
    return _status(max(valid))


def _pct_str(pct):
    """Format percentage or dash if None (7 chars wide)."""
    if pct is None:
        return "    —  "
    return f"{pct:6.1f}%"


def print_overview(platform_rows, op_totals, features_by_platform):
    """Print the default overview: requirements summary + unified comparison table.

    Args:
        platform_rows: [{key, platform, timing, min_clock, build_data, features}]
        op_totals: {tier: {mul, add, div, trig, sqrt}}
        features_by_platform: {platform_key: features_dict}
    """
    from .calc import calculate_recommended_clock

    # Header
    print(f"\n{'=' * 90}")
    print(f"  COMPLEXITY ANALYSIS")
    print(f"{'=' * 90}")

    # Source ops summary
    base_ops = op_totals.get("base", {})
    base_total = sum(base_ops.values())
    opt_ops = op_totals.get("optimization", {})
    opt_total = sum(opt_ops.values())
    race_ops = op_totals.get("racing", {})
    race_total = sum(race_ops.values())

    # Determine active features from any platform (they're the same config flags)
    sample_features = next(iter(features_by_platform.values()), {})
    active = []
    if sample_features.get("use_optimization"):
        active.append("OPTIMIZATION")
    if sample_features.get("use_racing"):
        active.append("RACING")
    if not active:
        active.append("base only")

    # Show distinct loop rates
    loop_rates = set()
    for row in platform_rows:
        loop_rates.add(row["features"]["loop_frequency_hz"])
    loop_str = " / ".join(f"{hz} Hz" for hz in sorted(loop_rates))

    print(f"  Loop: {loop_str} | Features: {', '.join(active)}")
    print(f"  Source: {base_total} base ops/loop ({base_ops.get('mul', 0)} mul, "
          f"{base_ops.get('add', 0)} add, {base_ops.get('div', 0)} div, "
          f"{base_ops.get('trig', 0)} trig, {base_ops.get('sqrt', 0)} sqrt)")
    if opt_total > 0 and sample_features.get("use_optimization"):
        print(f"          +{opt_total} optimization ops")
    if race_total > 0 and sample_features.get("use_racing"):
        print(f"          +{race_total} racing ops")

    # Min clock range across FPU platforms
    fpu_mins = [r["min_clock"] for r in platform_rows if r["platform"].fpu and r["min_clock"] < float('inf')]
    if fpu_mins:
        lo, hi = min(fpu_mins), max(fpu_mins)
        if lo == hi:
            print(f"  Min clock (FPU): {lo:.1f} MHz")
        else:
            print(f"  Min clock (FPU): {lo:.1f}–{hi:.1f} MHz (varies by I/O speed)")

    # Unified comparison table
    has_memory = any(r.get("build_data") and "error" not in r["build_data"] for r in platform_rows)

    print()
    if has_memory:
        hdr = f"  {'Platform':<38} {'CPU':>6} {'Min MHz':>8} {'Flash':>7} {'RAM':>7} {'Status':>8}"
        sep = f"  {'-' * 38} {'-' * 6} {'-' * 8} {'-' * 7} {'-' * 7} {'-' * 8}"
    else:
        hdr = f"  {'Platform':<38} {'CPU':>6} {'Min MHz':>8} {'Status':>8}"
        sep = f"  {'-' * 38} {'-' * 6} {'-' * 8} {'-' * 8}"
    print(hdr)
    print(sep)

    for row in platform_rows:
        plat = row["platform"]
        timing = row["timing"]
        cpu_pct = timing["utilization"] * 100
        min_clk = row["min_clock"]

        flash_pct = None
        ram_pct = None
        bd = row.get("build_data")
        if bd and "error" not in bd:
            flash_pct = bd["pct"]["flash_pct"]
            ram_pct = bd["pct"]["ram_pct"]

        status = _worst_status(cpu_pct, ram_pct)
        min_str = f"{min_clk:>8.1f}" if min_clk < float('inf') else "     inf"

        if has_memory:
            print(f"  {plat.name:<38} {cpu_pct:5.1f}% {min_str} {_pct_str(flash_pct)} {_pct_str(ram_pct)} {status:>8}")
        else:
            print(f"  {plat.name:<38} {cpu_pct:5.1f}% {min_str} {status:>8}")

    print()
    print(f"  Detailed: -p <platform>   Custom: --clock <MHz> --cores <N> --fpu/--no-fpu")


def print_cpu_report(timing, platform, platform_key, features):
    """Print CPU timing analysis."""
    print(f"\n{'=' * 70}")
    print(f"  CPU ANALYSIS: {platform.name}")
    print(f"{'=' * 70}")
    print(f"  Clock: {platform.clock_mhz:.0f} MHz | Cores: {platform.cores} | FPU: {'yes' if platform.fpu else 'no'}")
    print(f"  Loop rate: {timing['loop_hz']} Hz ({timing['loop_budget_us']:.0f} us budget)")

    # Feature list
    active = []
    if features.get("use_optimization"):
        active.append("OPTIMIZATION")
    if features.get("use_racing"):
        active.append("RACING")
    if not active:
        active.append("base only")
    print(f"  Features: {', '.join(active)}")

    # Tier breakdown
    print(f"\n  {'Tier':<22} {'Ops':>8} {'Cycles':>10} {'Time':>10}")
    print(f"  {'-' * 22} {'-' * 8} {'-' * 10} {'-' * 10}")

    for t in timing["tiers"]:
        ops = t["ops"]
        total_ops = sum(ops.values())
        print(f"  {t['name']:<22} {total_ops:>8} {t['cycles']:>10} {t['compute_us']:>9.1f}us")

    print(f"  {'-' * 22} {'-' * 8} {'-' * 10} {'-' * 10}")
    print(f"  {'I/O (bus + PWM)':<22} {'':>8} {'':>10} {timing['io_us']:>9.1f}us")
    print(f"  {'Overhead (20%)':<22} {'':>8} {'':>10} {timing['overhead_us']:>9.1f}us")
    print(f"  {'TOTAL':<22} {'':>8} {timing['total_cycles']:>10} {timing['total_us']:>9.1f}us")

    # Utilization
    util_pct = timing["utilization"] * 100
    print(f"\n  Utilization: {_bar(util_pct)} {util_pct:5.1f}%  {_status(util_pct)}")
    print(f"  Headroom: {timing['headroom_us']:.0f}us per loop")

    if util_pct > 100:
        print(f"  CRITICAL: Loop CANNOT run at {timing['loop_hz']} Hz on this platform!")
    elif util_pct > 80:
        print(f"  WARNING: CPU utilization > 80% — loop may not hit target rate")


def print_memory_report(build_data):
    """Print memory analysis from build artifacts."""
    if "error" in build_data:
        print(f"\n  MEMORY: {build_data['error']}")
        return

    size = build_data["size"]
    limits = build_data["limits"]
    pct = build_data["pct"]

    print(f"\n{'=' * 70}")
    print(f"  MEMORY ANALYSIS: {build_data['env']}")
    print(f"{'=' * 70}")

    if build_data.get("elf_age_s", 0) > 3600:
        age_h = build_data["elf_age_s"] / 3600
        print(f"  Note: Build is {age_h:.1f}h old. Rebuild for current analysis.")

    flash_pct = pct["flash_pct"]
    ram_pct = pct["ram_pct"]

    print(f"\n  Flash: {_bar(flash_pct)} {flash_pct:5.1f}%  ({_format_bytes(size['flash_used'])} / {_format_bytes(limits['flash'])})")
    print(f"  RAM:   {_bar(ram_pct)} {ram_pct:5.1f}%  ({_format_bytes(size['ram_used'])} / {_format_bytes(limits['ram'])})")

    print(f"\n  {'Section':<12} {'Size':>10}")
    print(f"  {'-' * 12} {'-' * 10}")
    print(f"  {'text':<12} {_format_bytes(size['text']):>10}  (code)")
    print(f"  {'data':<12} {_format_bytes(size['data']):>10}  (initialized globals)")
    print(f"  {'bss':<12} {_format_bytes(size['bss']):>10}  (zero-init globals)")

    # Top symbols
    if build_data.get("top_symbols"):
        print(f"\n  Largest symbols:")
        for sym in reversed(build_data["top_symbols"][-10:]):
            stype = {"T": "code", "t": "code", "D": "data", "d": "data",
                     "B": "bss", "b": "bss", "R": "rodata", "r": "rodata"}.get(sym["type"], sym["type"])
            print(f"    {_format_bytes(sym['size']):>8}  {stype:<6}  {sym['name']}")


def print_source_report(per_file, totals):
    """Print source code FP operation scan results."""
    print(f"\n{'=' * 70}")
    print(f"  SOURCE SCAN: Floating-Point Operations")
    print(f"{'=' * 70}")
    print(f"  Scanned source files for arithmetic operators and math functions.")
    print(f"  Note: Heuristic count — includes some integer ops. Use as relative guide.\n")

    print(f"  {'Tier':<16} {'mul':>6} {'add':>6} {'div':>6} {'trig':>6} {'sqrt':>6} {'total':>8}")
    print(f"  {'-' * 16} {'-' * 6} {'-' * 6} {'-' * 6} {'-' * 6} {'-' * 6} {'-' * 8}")

    grand_total = 0
    for tier in ["base", "optimization", "racing", "calibration"]:
        if tier in totals:
            ops = totals[tier]
            total = sum(ops.values())
            grand_total += total
            label = tier.upper() if tier != "base" else "Base"
            print(f"  {label:<16} {ops['mul']:>6} {ops['add']:>6} {ops['div']:>6} {ops['trig']:>6} {ops['sqrt']:>6} {total:>8}")

    print(f"  {'-' * 16} {'-' * 6} {'-' * 6} {'-' * 6} {'-' * 6} {'-' * 6} {'-' * 8}")
    print(f"  {'TOTAL':<16} {'':>6} {'':>6} {'':>6} {'':>6} {'':>6} {grand_total:>8}")

    # Per-file breakdown
    print(f"\n  Per-file breakdown (base tier only, top files):")
    base_files = []
    for fname, tiers in per_file.items():
        if "base" in tiers:
            total = sum(tiers["base"].values())
            base_files.append((total, fname))
    base_files.sort(reverse=True)

    for total, fname in base_files[:8]:
        print(f"    {total:>6} ops  {fname}")
