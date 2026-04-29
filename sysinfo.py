#!/usr/bin/env python3
"""
sysinfo.py — Sistem & Container Bilgi Toplayici
Kullanim: python3 sysinfo.py
Gereksinim: psutil (yoksa otomatik kurar)
"""

import subprocess
import sys
import os
import platform
import datetime
import pathlib
import socket


# psutil yoksa otomatik kur
try:
    import psutil
except ImportError:
    print("[*] psutil bulunamadi, kuruluyor...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "psutil", "-q"])
    import psutil


SEP = "=" * 55


def section(title):
    print(f"\n{SEP}\n  {title}\n{SEP}")


def bytes_to_gb(b):
    return round(b / (1024 ** 3), 2)


def read_file(path):
    try:
        return pathlib.Path(path).read_text(errors="ignore").strip()
    except Exception:
        return None


def run_cmd(cmd):
    try:
        return subprocess.check_output(
            cmd, shell=True, stderr=subprocess.DEVNULL, text=True
        ).strip()
    except Exception:
        return None


# ─── CONTAINER TESPİTİ ───────────────────────────────────────────────────────

def detect_container():
    section("CONTAINER / ORTAM TESPİTİ")
    found = []

    if pathlib.Path("/.dockerenv").exists():
        found.append("Docker  (.dockerenv mevcut)")
    if pathlib.Path("/run/.containerenv").exists():
        found.append("Podman  (.containerenv mevcut)")

    cgroup = read_file("/proc/1/cgroup")
    if cgroup:
        if "docker" in cgroup:
            found.append("Docker  (cgroup kaydinda)")
        if "kubepods" in cgroup:
            found.append("Kubernetes (cgroup kaydinda)")
        if "lxc" in cgroup:
            found.append("LXC container (cgroup kaydinda)")

    if os.environ.get("KUBERNETES_SERVICE_HOST"):
        found.append("Kubernetes (env: KUBERNETES_SERVICE_HOST)")
    if os.environ.get("container"):
        found.append(f"Systemd container (env: container={os.environ['container']})")

    if found:
        print("Tespit edilenler:")
        for f in found:
            print(f"  [+] {f}")
    else:
        print("  Container belirtisi yok — bare-metal veya VM olabilir")

    # Namespace bilgisi
    ns = run_cmd("ls -la /proc/1/ns/ 2>/dev/null | head -10")
    if ns:
        print(f"\nNamespace'ler:\n{ns}")


# ─── HOST OS ERİŞİMİ ─────────────────────────────────────────────────────────

def host_os_info():
    section("HOST OS BİLGİSİ (/proc/1/root)")
    host_root = pathlib.Path("/proc/1/root")

    if not host_root.exists():
        print("  /proc/1/root erişilemiyor")
        return

    for rel in ["etc/os-release", "etc/issue", "etc/hostname", "etc/debian_version",
                "etc/redhat-release", "etc/alpine-release"]:
        content = read_file(host_root / rel)
        if content:
            print(f"\n[{rel}]\n{content}")

    print("\nHost kök dizin listesi:")
    try:
        for entry in sorted(host_root.iterdir()):
            tag = "DIR " if entry.is_dir() else "FILE"
            print(f"  {tag}  {entry.name}")
    except PermissionError:
        print("  (Erişim izni yok — privileged gerekebilir)")


# ─── OS / PLATFORM ───────────────────────────────────────────────────────────

def os_info():
    section("MEVCUT OS / PLATFORM")
    uname = platform.uname()
    print(f"Sistem   : {uname.system}")
    print(f"Sürüm    : {uname.release} / {uname.version}")
    print(f"Hostname : {uname.node}")
    print(f"Arch     : {uname.machine}")
    print(f"Python   : {platform.python_version()} ({sys.executable})")
    print(f"Tarih    : {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Uptime   : {run_cmd('uptime -p') or 'N/A'}")

    content = read_file("/etc/os-release")
    if content:
        print("\n/etc/os-release:")
        for line in content.splitlines():
            if any(k in line for k in ("PRETTY_NAME", "NAME=", "VERSION=")):
                print(f"  {line}")


# ─── CPU ─────────────────────────────────────────────────────────────────────

def cpu_info():
    section("CPU")
    print(f"Fiziksel core : {psutil.cpu_count(logical=False)}")
    print(f"Mantıksal core: {psutil.cpu_count(logical=True)}")
    print(f"Kullanım      : %{psutil.cpu_percent(interval=1)}")

    freq = psutil.cpu_freq()
    if freq:
        print(f"Frekans       : {round(freq.current)} MHz  "
              f"(min:{round(freq.min)} max:{round(freq.max)})")

    load = os.getloadavg() if hasattr(os, "getloadavg") else None
    if load:
        print(f"Load avg      : {load[0]:.2f} / {load[1]:.2f} / {load[2]:.2f}  (1/5/15 dk)")

    # CPU model
    model = run_cmd("grep -m1 'model name' /proc/cpuinfo | cut -d: -f2")
    if model:
        print(f"Model         : {model.strip()}")


# ─── BELLEK ──────────────────────────────────────────────────────────────────

def memory_info():
    section("BELLEK")
    mem = psutil.virtual_memory()
    swap = psutil.swap_memory()

    print(f"RAM  Toplam   : {bytes_to_gb(mem.total):>8} GB")
    print(f"RAM  Kullanım : {bytes_to_gb(mem.used):>8} GB  (%{mem.percent})")
    print(f"RAM  Boş      : {bytes_to_gb(mem.available):>8} GB")
    print(f"Swap Toplam   : {bytes_to_gb(swap.total):>8} GB")
    print(f"Swap Kullanım : {bytes_to_gb(swap.used):>8} GB  (%{swap.percent})")


# ─── DİSK ────────────────────────────────────────────────────────────────────

def disk_info():
    section("DİSK / MOUNT NOKTALARI")
    print(f"{'Mount':<20} {'Toplam':>9} {'Dolu':>9} {'Boş':>9} {'%':>5}  Tip")
    print("-" * 65)
    for part in psutil.disk_partitions(all=False):
        try:
            usage = psutil.disk_usage(part.mountpoint)
            print(f"{part.mountpoint:<20} "
                  f"{bytes_to_gb(usage.total):>8}G "
                  f"{bytes_to_gb(usage.used):>8}G "
                  f"{bytes_to_gb(usage.free):>8}G "
                  f"{usage.percent:>5}%  {part.fstype}")
        except (PermissionError, OSError):
            pass


# ─── AĞ ──────────────────────────────────────────────────────────────────────

def network_info():
    section("AĞ ARAYÜZLERİ")
    for iface, addrs in psutil.net_if_addrs().items():
        for addr in addrs:
            if addr.family == socket.AF_INET:
                print(f"  {iface:<16} IPv4 : {addr.address}")
            elif addr.family == socket.AF_INET6:
                print(f"  {iface:<16} IPv6 : {addr.address}")

    print(f"\nHostname : {socket.gethostname()}")
    try:
        print(f"Dış IP   : {run_cmd('curl -s --max-time 3 ifconfig.me') or 'N/A'}")
    except Exception:
        pass

    print(f"\nAktif bağlantılar: {len(psutil.net_connections())}")

    # DNS
    resolv = read_file("/etc/resolv.conf")
    if resolv:
        print("\n/etc/resolv.conf:")
        for line in resolv.splitlines():
            if line and not line.startswith("#"):
                print(f"  {line}")


# ─── SÜREÇLER ────────────────────────────────────────────────────────────────

def process_info():
    section("EN FAZLA BELLEK KULLANAN 15 SÜREÇ")
    procs = sorted(
        psutil.process_iter(["pid", "name", "cpu_percent", "memory_percent", "username"]),
        key=lambda p: p.info.get("memory_percent") or 0,
        reverse=True
    )
    print(f"{'PID':<8} {'Kullanıcı':<12} {'Ad':<28} {'CPU%':>6} {'MEM%':>6}")
    print("-" * 65)
    for p in procs[:15]:
        info = p.info
        print(f"{info['pid']:<8} "
              f"{(info.get('username') or '?'):<12} "
              f"{(info['name'] or '?'):<28} "
              f"{info.get('cpu_percent') or 0:>6.1f} "
              f"{info.get('memory_percent') or 0:>6.2f}")


# ─── ORTAM DEĞİŞKENLERİ ──────────────────────────────────────────────────────

def env_info():
    section("ÖNEMLİ ORTAM DEĞİŞKENLERİ")
    keys = [
        "PATH", "HOME", "USER", "HOSTNAME",
        "KUBERNETES_SERVICE_HOST", "KUBERNETES_PORT",
        "POD_NAME", "POD_NAMESPACE", "NODE_NAME",
        "DOCKER_HOST", "container",
        "LANG", "TZ",
    ]
    for k in keys:
        v = os.environ.get(k)
        if v:
            # PATH çok uzunsa kısalt
            if k == "PATH" and len(v) > 80:
                v = v[:80] + "..."
            print(f"  {k:<30} = {v}")


# ─── MAIN ────────────────────────────────────────────────────────────────────

def main():
    print(f"\n{'#' * 55}")
    print(f"#  SYSINFO — {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'#' * 55}")

    detect_container()
    host_os_info()
    os_info()
    cpu_info()
    memory_info()
    disk_info()
    network_info()
    process_info()
    env_info()

    print(f"\n{SEP}\n  TAMAMLANDI\n{SEP}\n")


if __name__ == "__main__":
    main()
