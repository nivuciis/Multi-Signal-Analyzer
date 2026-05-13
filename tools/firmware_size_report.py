#!/usr/bin/env python3
"""
Firmware Size Report Generator
===============================
Analyzes the firmware binary size and generates a detailed report with:
  - Memory usage breakdown (text, data, bss)
  - Percentage of flash/RAM utilization
  - Timestamp of build
  - Warnings if limits are exceeded
  - Historical log tracking

Usage:
    python3 firmware_size_report.py <binary_path> [--limits] [--log]

    python3 firmware_size_report.py build/multi_signal_analyzer.elf
    python3 firmware_size_report.py build/multi_signal_analyzer.elf --log build/size_history.csv

Requires:
    arm-none-eabi-size (from ARM toolchain)
"""

import subprocess
import sys
import os
from datetime import datetime
from pathlib import Path

# RP2040/RP2350 Memory Limits
FLASH_SIZE = 2 * 1024 * 1024  # 2 MB
SRAM_SIZE = 264 * 1024  # 264 KB
BOOT_ROM_SIZE = 16 * 1024  # 16 KB (reserved for bootloader)

# Warning thresholds (percentage of available space)
FLASH_WARNING_THRESHOLD = 80
SRAM_WARNING_THRESHOLD = 85
CRITICAL_THRESHOLD = 95

# Color codes for terminal output
class Colors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def format_bytes(num_bytes):
    """Convert bytes to human-readable format."""
    for unit in ['B', 'KB', 'MB']:
        if num_bytes < 1024.0:
            return f"{num_bytes:.1f} {unit}"
        num_bytes /= 1024.0
    return f"{num_bytes:.1f} GB"


def get_firmware_size(binary_path):
    """Execute arm-none-eabi-size and parse output."""
    try:
        result = subprocess.run(
            ['arm-none-eabi-size', '-B', binary_path],
            capture_output=True,
            text=True,
            check=True
        )
        
        lines = result.stdout.strip().split('\n')
        if len(lines) < 2:
            raise ValueError("Unexpected size output format")
        
        # Parse header and data
        # Format: text    data    bss     dec     hex     filename
        data = lines[1].split()
        return {
            'text': int(data[0]),      # Code/Flash
            'data': int(data[1]),      # Initialized data in RAM
            'bss': int(data[2]),       # Uninitialized data in RAM
            'dec': int(data[3]),
            'hex': data[4],
        }
    except subprocess.CalledProcessError as e:
        print(f"{Colors.RED}Error: Failed to execute arm-none-eabi-size{Colors.ENDC}")
        print(e.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print(f"{Colors.RED}Error: arm-none-eabi-size not found. Please install ARM toolchain.{Colors.ENDC}")
        sys.exit(1)


def calculate_usage(size_data):
    """Calculate memory usage percentages and available space."""
    flash_used = size_data['text'] + size_data['data']
    ram_used = size_data['data'] + size_data['bss']
    
    flash_available = FLASH_SIZE - BOOT_ROM_SIZE
    ram_available = SRAM_SIZE
    
    flash_percent = (flash_used / flash_available) * 100
    ram_percent = (ram_used / ram_available) * 100
    
    return {
        'flash_used': flash_used,
        'flash_available': flash_available,
        'flash_percent': flash_percent,
        'ram_used': ram_used,
        'ram_available': ram_available,
        'ram_percent': ram_percent,
    }


def get_status_color(percent, warning_threshold=80, critical_threshold=95):
    """Return color based on usage percentage."""
    if percent >= critical_threshold:
        return Colors.RED
    elif percent >= warning_threshold:
        return Colors.YELLOW
    else:
        return Colors.GREEN


def print_report(binary_path, size_data, usage_data):
    """Print formatted size report."""
    print(f"\n{Colors.BOLD}{Colors.CYAN}╔══════════════════════════════════════════════════════════════╗{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.CYAN}║         FIRMWARE SIZE REPORT - Multi-Signal Analyzer          ║{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.CYAN}╚══════════════════════════════════════════════════════════════╝{Colors.ENDC}\n")
    
    # Timestamp
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"  {Colors.BLUE}Build Time:{Colors.ENDC}  {timestamp}")
    print(f"  {Colors.BLUE}Binary:{Colors.ENDC}       {binary_path}\n")
    
    # Memory breakdown
    print(f"{Colors.BOLD}Memory Breakdown:{Colors.ENDC}")
    print(f"  {Colors.BLUE}Code (Flash):{Colors.ENDC}      {format_bytes(size_data['text']):>12} ({size_data['text']:>8} B)")
    print(f"  {Colors.BLUE}Init Data (RAM):{Colors.ENDC}   {format_bytes(size_data['data']):>12} ({size_data['data']:>8} B)")
    print(f"  {Colors.BLUE}Uninit Data (BSS):{Colors.ENDC} {format_bytes(size_data['bss']):>12} ({size_data['bss']:>8} B)")
    
    total_size = size_data['dec']
    print(f"  {Colors.BOLD}{Colors.BLUE}Total:{Colors.ENDC}              {format_bytes(total_size):>12} ({total_size:>8} B)\n")
    
    # Flash usage
    flash_color = get_status_color(usage_data['flash_percent'])
    flash_bar = _create_progress_bar(usage_data['flash_percent'])
    print(f"{Colors.BOLD}Flash Memory:{Colors.ENDC}")
    print(f"  {flash_color}{format_bytes(usage_data['flash_used']):>8}{Colors.ENDC} / {format_bytes(usage_data['flash_available'])} "
          f"({flash_color}{usage_data['flash_percent']:>5.1f}%{Colors.ENDC})")
    print(f"  {flash_bar}")
    
    if usage_data['flash_percent'] >= 95:
        print(f"  {Colors.RED}{Colors.BOLD}⚠ CRITICAL: Flash nearly full!{Colors.ENDC}")
    elif usage_data['flash_percent'] >= 80:
        print(f"  {Colors.YELLOW}⚠ WARNING: Flash usage above 80%{Colors.ENDC}")
    
    # RAM usage
    ram_color = get_status_color(usage_data['ram_percent'])
    ram_bar = _create_progress_bar(usage_data['ram_percent'])
    print(f"\n{Colors.BOLD}RAM Memory:{Colors.ENDC}")
    print(f"  {ram_color}{format_bytes(usage_data['ram_used']):>8}{Colors.ENDC} / {format_bytes(usage_data['ram_available'])} "
          f"({ram_color}{usage_data['ram_percent']:>5.1f}%{Colors.ENDC})")
    print(f"  {ram_bar}")
    
    if usage_data['ram_percent'] >= 95:
        print(f"  {Colors.RED}{Colors.BOLD}⚠ CRITICAL: RAM nearly full!{Colors.ENDC}")
    elif usage_data['ram_percent'] >= 85:
        print(f"  {Colors.YELLOW}⚠ WARNING: RAM usage above 85%{Colors.ENDC}")
    
    print(f"\n{Colors.CYAN}─────────────────────────────────────────────────────────────────{Colors.ENDC}\n")


def _create_progress_bar(percent, width=40):
    """Create a visual progress bar."""
    filled = int(width * percent / 100)
    bar = '█' * filled + '░' * (width - filled)
    
    color = get_status_color(percent)
    return f"  [{color}{bar}{Colors.ENDC}] {percent:.1f}%"


def log_to_file(log_file, binary_path, size_data, usage_data):
    """Append size information to CSV log file."""
    log_path = Path(log_file)
    timestamp = datetime.now().isoformat()
    
    # Create header if file doesn't exist
    if not log_path.exists():
        log_path.parent.mkdir(parents=True, exist_ok=True)
        with open(log_path, 'w') as f:
            f.write("timestamp,text_bytes,data_bytes,bss_bytes,total_bytes,")
            f.write("flash_percent,ram_percent,binary\n")
    
    # Append entry
    with open(log_path, 'a') as f:
        f.write(f"{timestamp},")
        f.write(f"{size_data['text']},")
        f.write(f"{size_data['data']},")
        f.write(f"{size_data['bss']},")
        f.write(f"{size_data['dec']},")
        f.write(f"{usage_data['flash_percent']:.1f},")
        f.write(f"{usage_data['ram_percent']:.1f},")
        f.write(f"{os.path.basename(binary_path)}\n")
    
    print(f"{Colors.GREEN}✓ Size history logged to: {log_file}{Colors.ENDC}\n")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <binary_path> [--log <log_file>]")
        sys.exit(1)
    
    binary_path = sys.argv[1]
    log_file = None
    
    # Parse optional arguments
    if '--log' in sys.argv:
        idx = sys.argv.index('--log')
        if idx + 1 < len(sys.argv):
            log_file = sys.argv[idx + 1]
    
    if not os.path.exists(binary_path):
        print(f"{Colors.RED}Error: Binary not found: {binary_path}{Colors.ENDC}")
        sys.exit(1)
    
    # Get and analyze size
    size_data = get_firmware_size(binary_path)
    usage_data = calculate_usage(size_data)
    
    # Display report
    print_report(binary_path, size_data, usage_data)
    
    # Log if requested
    if log_file:
        log_to_file(log_file, binary_path, size_data, usage_data)
    
    # Exit with warning code if thresholds exceeded
    if usage_data['flash_percent'] >= 95 or usage_data['ram_percent'] >= 95:
        sys.exit(2)  # Critical
    elif usage_data['flash_percent'] >= 80 or usage_data['ram_percent'] >= 85:
        sys.exit(1)  # Warning


if __name__ == "__main__":
    main()
