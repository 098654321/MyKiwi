#!/usr/bin/env python3
import json
import os
import argparse
import sys

def parse_controlbits(controlbits_path):
    """Parses the controlbits file into a dictionary of {name: value}."""
    control_map = {}
    try:
        with open(controlbits_path, 'r') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                parts = line.split()
                if len(parts) >= 2:
                    # Format: value name
                    value = parts[0]
                    name = parts[1]
                    control_map[name] = value
                else:
                    print(f"Warning: Skipping invalid line {line_num} in {controlbits_path}: {line}")
    except FileNotFoundError:
        print(f"Error: Controlbits file not found: {controlbits_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading controlbits file: {e}")
        sys.exit(1)
        
    return control_map

def load_register_map(json_path):
    """Loads the register map JSON file."""
    try:
        with open(json_path, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: Register map JSON file not found: {json_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading JSON file: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Split controlbits file into multiple register files based on JSON map.")
    parser.add_argument("--controlbits", "-c", required=True, help="Path to the input controlbits file (e.g., controlbits_0.txt)")
    parser.add_argument("--json_map", "-j", required=True, help="Path to the register map JSON file")
    parser.add_argument("--output_dir", "-o", help="Directory to save the output files. Defaults to the same directory as controlbits file.")
    
    args = parser.parse_args()
    
    controlbits_path = args.controlbits
    json_path = args.json_map
    output_dir = args.output_dir if args.output_dir else os.path.dirname(controlbits_path)
    
    if not os.path.exists(output_dir):
        try:
            os.makedirs(output_dir)
        except OSError as e:
            print(f"Error creating output directory: {e}")
            sys.exit(1)

    # Setup logging to report.log
    log_file_path = os.path.join(output_dir, "report.log")
    
    class Logger(object):
        def __init__(self, filename):
            self.terminal = sys.stdout
            self.log = open(filename, "w")

        def write(self, message):
            self.terminal.write(message)
            self.log.write(message)
            self.log.flush()

        def flush(self):
            # this flush method is needed for python 3 compatibility.
            # this handles the flush command by doing nothing.
            # you might want to specify some extra behavior here.
            self.terminal.flush()
            self.log.flush()

    sys.stdout = Logger(log_file_path)

    print(f"Loading register map from {json_path}...")
    reg_map = load_register_map(json_path)
    
    print(f"Loading controlbits from {controlbits_path}...")
    control_data = parse_controlbits(controlbits_path)
    
    # Track used registers in controlbits
    used_control_regs = set()
    
    # Track missing registers in controlbits (present in JSON but not in controlbits)
    missing_in_controlbits = []
    
    print("\nProcessing files...")
    
    for filename, regs in reg_map.items():
        output_file_path = os.path.join(output_dir, filename)
        print(f"Generating {output_file_path}...")
        
        try:
            with open(output_file_path, 'w') as out_f:
                for reg_name, address in regs.items():
                    if reg_name in control_data:
                        value = control_data[reg_name]
                        # Write format: value address name
                        out_f.write(f"{value} {address} {reg_name}\n")
                        used_control_regs.add(reg_name)
                    else:
                        missing_in_controlbits.append((filename, reg_name))
                        # Optional: write a placeholder or comment? 
                        # For now, let's just log it and skip writing to keep the file valid/clean?
                        # Or maybe write with a default value? The prompt implies "拆解", so we expect them to be there.
                        # If missing, we can't write a valid line.
                        pass
        except Exception as e:
            print(f"Error writing to {output_file_path}: {e}")

    # Report results
    print("\n" + "="*40)
    print("REPORT")
    print("="*40)

    # 1. Registers in JSON but missing in controlbits
    if missing_in_controlbits:
        print(f"\n[WARNING] The following registers are defined in JSON but MISSING in {os.path.basename(controlbits_path)}:")
        for fname, rname in missing_in_controlbits:
            print(f"  - {rname} (expected in {fname})")
    else:
        print(f"\n[OK] All registers in JSON were found in {os.path.basename(controlbits_path)}.")

    # 2. Registers in controlbits but missing in JSON
    all_control_regs = set(control_data.keys())
    extra_regs = all_control_regs - used_control_regs
    
    if extra_regs:
        print(f"\n[WARNING] The following registers are present in {os.path.basename(controlbits_path)} but NOT used in any JSON file mapping:")
        for rname in sorted(extra_regs):
            print(f"  - {rname}")
    else:
        print(f"\n[OK] All registers in {os.path.basename(controlbits_path)} were mapped to files.")

    print("\nDone.")

if __name__ == "__main__":
    main()
