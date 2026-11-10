#!/usr/bin/env python3
import sys
from pathlib import Path
out=Path(sys.argv[1]);images=[Path(p).read_bytes() for p in sys.argv[2:]]
out.write_bytes(max(images,key=len))
