#!/usr/bin/env python3
"""Generate the NVS partition image that pre-seeds the e2e firmware's config
storage with the reference aircraft from the shared API fixture
(test/fixtures/api/config.json).

With an erased NVS partition (the previous state of the QEMU run),
config_init()'s writes silently fail, the firmware boots unconfigured, and
the e2e ended up asserting the breakage ("slot is empty - no identity
configured"). Seeding the keys means the firmware boots as a configured
aircraft - the same aircraft the ODID reference fixtures encode - and
config_init() needs no writes at all.

Usage: nvs_seed.py <output.bin> <partition size>   (run inside the e2e image)
"""

import csv
import json
import os
import subprocess
import sys


def main():
    workspace = os.environ.get("DRIFT_WORKSPACE", "/work")
    config_path = os.path.join(workspace, "test/fixtures/api/config.json")
    out_bin = sys.argv[1] if len(sys.argv) > 1 else "nvs-seed.bin"
    size = sys.argv[2] if len(sys.argv) > 2 else "0x5000"

    with open(config_path) as f:
        config = json.load(f)

    rows = [
        ["key", "type", "encoding", "value"],
        ["drift", "namespace", "", ""],
        ["wifi_ssid", "data", "string", config["wifi"]["ssid"]],
        ["wifi_password", "data", "string", config["wifi"]["password"]],
        ["dri_ua_id", "data", "string", config["dri"]["ua_id"]],
        ["dri_ua_desc", "data", "string", config["dri"]["ua_desc"]],
        ["dri_op_id", "data", "string", config["dri"]["op_id"]],
    ]
    csv_path = out_bin + ".csv"
    with open(csv_path, "w", newline="") as f:
        csv.writer(f).writerows(rows)

    subprocess.run([sys.executable, "-m", "esp_idf_nvs_partition_gen",
                    "generate", csv_path, out_bin, size], check=True)


if __name__ == "__main__":
    main()
