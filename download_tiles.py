import os, math, urllib.request, concurrent.futures, time, sys

tile_dirs = [
    os.path.join(os.environ.get("LOCALAPPDATA", "C:/Users/Whaoy/AppData/Local"), "PosNavTech", "tiles"),
    r"c:\Users\Whaoy\Documents\win-realtime\gps-ui-qt\gps-ui-qt_portable\tiles",
    r"c:\Users\Whaoy\Documents\win-realtime\全能\tiles",
    r"c:\Users\Whaoy\Documents\win-realtime\高端\tiles",
    r"c:\Users\Whaoy\Documents\win-realtime\简单版\tiles",
]
for d in tile_dirs:
    os.makedirs(d, exist_ok=True)

WEST, EAST = 73.66, 135.05
SOUTH, NORTH = 18.16, 53.56

def lon_to_tile_x(lon, z):
    return int((lon + 180.0) / 360.0 * (1 << z))

def lat_to_tile_y(lat, z):
    return int((1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * (1 << z))

stats = {"ok": 0, "fail": 0, "skip": 0}

def save_to_all(data, z, x, y):
    for td in tile_dirs:
        tf = os.path.join(td, str(z), str(x), f"{y}.png")
        os.makedirs(os.path.dirname(tf), exist_ok=True)
        with open(tf, "wb") as f:
            f.write(data)

def download_tile(z, x, y):
    ref_dir = tile_dirs[0]
    ref_file = os.path.join(ref_dir, str(z), str(x), f"{y}.png")
    if os.path.exists(ref_file) and os.path.getsize(ref_file) > 100:
        stats["skip"] += 1
        return True
    url = f"https://tile.openstreetmap.org/{z}/{x}/{y}.png"
    req = urllib.request.Request(url, headers={"User-Agent": "GPS-Realtime/1.0; contact=none"})
    for attempt in range(2):
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                data = resp.read()
                if len(data) > 100:
                    save_to_all(data, z, x, y)
                    stats["ok"] += 1
                    return True
        except Exception:
            pass
    # try French mirror
    url2 = f"https://tile-a.openstreetmap.fr/hot/{z}/{x}/{y}.png"
    req2 = urllib.request.Request(url2, headers={"User-Agent": "GPS-Realtime/1.0; contact=none"})
    for attempt in range(2):
        try:
            with urllib.request.urlopen(req2, timeout=10) as resp:
                data = resp.read()
                if len(data) > 100:
                    save_to_all(data, z, x, y)
                    stats["ok"] += 1
                    return True
        except Exception:
            pass
    stats["fail"] += 1
    return False

def show_progress(current, total, label=""):
    bar_len = 30
    filled = int(bar_len * current / total) if total > 0 else 0
    bar = "=" * filled + ">" * (1 if filled < bar_len else 0) + " " * (bar_len - filled - (1 if filled < bar_len else 0))
    pct = current * 100 / total if total > 0 else 0
    sys.stdout.write(f"\r{label} [{bar}] {current}/{total} ({pct:.0f}%)  ok:{stats['ok']} fail:{stats['fail']} skip:{stats['skip']}   ")
    sys.stdout.flush()

start_time = time.time()
for z in range(9, 10):
    x_min = max(0, lon_to_tile_x(WEST, z))
    x_max = min((1 << z) - 1, lon_to_tile_x(EAST, z))
    y_min = max(0, lat_to_tile_y(NORTH, z))
    y_max = min((1 << z) - 1, lat_to_tile_y(SOUTH, z))
    tiles = [(z, x, y) for x in range(x_min, x_max + 1) for y in range(y_min, y_max + 1)]
    done = 0
    show_progress(0, len(tiles), f"z{z}")
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
        futures = {ex.submit(download_tile, *t): t for t in tiles}
        for f in concurrent.futures.as_completed(futures):
            done += 1
            show_progress(done, len(tiles), f"z{z}")
    elapsed = time.time() - start_time
    sys.stdout.write(f"\n  -> {elapsed:.0f}s elapsed\n")

# Also download z10-z12 for major cities
cities = [
    ("Beijing", 116.4, 39.9),
    ("Shanghai", 121.5, 31.2),
    ("Guangzhou", 113.3, 23.1),
    ("Shenzhen", 114.1, 22.5),
    ("Chengdu", 104.1, 30.6),
]
for name, clon, clat in cities:
    for z in range(10, 13):
        x0, y0 = lon_to_tile_x(clon, z), lat_to_tile_y(clat, z)
        r = 3
        tiles = [(z, x, y) for x in range(x0-r, x0+r+1) for y in range(y0-r, y0+r+1)]
        done = 0
        show_progress(0, len(tiles), f"{name} z{z}")
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
            futures = {ex.submit(download_tile, *t): t for t in tiles}
            for f in concurrent.futures.as_completed(futures):
                done += 1
                show_progress(done, len(tiles), f"{name} z{z}")
        sys.stdout.write(f"\n")

sys.stdout.write(f"\nDone! ok:{stats['ok']} fail:{stats['fail']} skip:{stats['skip']} total:{time.time()-start_time:.0f}s\n")
