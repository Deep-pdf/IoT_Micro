#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${IDF_PATH:-}" ]]; then
    echo "IDF_PATH chưa được thiết lập. Hãy chạy: . /duong-dan/esp-idf/export.sh" >&2
    exit 1
fi

if ! command -v idf.py >/dev/null 2>&1; then
    echo "Không tìm thấy idf.py trong PATH. Hãy kích hoạt môi trường ESP-IDF trước." >&2
    exit 1
fi

python3 rg_tool.py --target=gmt024-s3 clean launcher retro-core
python3 rg_tool.py --target=gmt024-s3 build-img launcher retro-core

echo
echo "Build hoàn tất. File firmware:"
find . -maxdepth 1 -name 'retro-go_*_gmt024-s3.img' -print
