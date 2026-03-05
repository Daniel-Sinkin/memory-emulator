#!/bin/bash
watchexec --watch src -- bash -c 'cmake --build build && clear && ./build/memory_emu'
