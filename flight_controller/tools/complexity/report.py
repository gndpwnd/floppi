"""
Report generation for complexity analysis.

Formats CPU timing, memory usage, and source scan results.
"""

from .platforms import PLATFORMS, get_cycles


def _bar(pct, width=20):
    """Generate ASCII progress bar."""
    filled = int(pct / 100 * width)
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


def print_comparison(features, op_totals, build_results=None):
    """Print comparison table across all platforms."""
    from .calc import calculate_loop_timing, calculate_min_clock, calculate_recommended_clock

    print(f"\n{'=' * 90}")
    print(f"  PLATFORM COMPARISON")
    print(f"{'=' * 90}")

    header = f"  {'Platform':<35} {'Clock':>7} {'Loop us':>8} {'Util':>7} {'Min MHz':>8} {'Status':>10}"
    print(header)
    print(f"  {'-' * 35} {'-' * 7} {'-' * 8} {'-' * 7} {'-' * 8} {'-' * 10}")

    for key, plat in PLATFORMS.items():
        timing = calculate_loop_timing(plat, key, features, op_totals)
        min_clk = calculate_min_clock(plat, key, features, op_totals)
        util_pct = timing["utilization"] * 100
        status = _status(util_pct)

        print(f"  {plat.name:<35} {plat.clock_mhz:>6.0f} {timing['total_us']:>7.0f}us {util_pct:>6.1f}% {min_clk:>7.1f} {status:>10}")

    # Memory comparison if builds available
    if build_results:
        print(f"\n  {'Environment':<25} {'Flash':>12} {'Flash %':>8} {'RAM':>12} {'RAM %':>8}")
        print(f"  {'-' * 25} {'-' * 12} {'-' * 8} {'-' * 12} {'-' * 8}")
        for br in build_results:
            if "error" not in br:
                s = br["size"]
                p = br["pct"]
                print(f"  {br['env']:<25} {_format_bytes(s['flash_used']):>12} {p['flash_pct']:>7.1f}% {_format_bytes(s['ram_used']):>12} {p['ram_pct']:>7.1f}%")


def print_clock_requirements(features, op_totals):
    """Print minimum and recommended clock speeds."""
    from .calc import calculate_min_clock, calculate_recommended_clock

    print(f"\n  Minimum clock requirements ({features['loop_frequency_hz']} Hz loop):")
    for key, plat in PLATFORMS.items():
        min_clk = calculate_min_clock(plat, key, features, op_totals)
        rec_clk = calculate_recommended_clock(min_clk)
        margin = plat.clock_mhz - min_clk
        status = "OK" if margin > 0 else "INSUFFICIENT"
        print(f"    {plat.name:<35} min: {min_clk:>6.1f} MHz  rec: {rec_clk:>6.1f} MHz  [{status}]")
