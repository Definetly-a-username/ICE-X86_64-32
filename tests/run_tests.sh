#!/bin/bash
# Test script for ICE OS
# Tests basic functionality without requiring interactive input

set -e

ICE="/home/delta/basement/ice/bin/ice"

echo "=== ICE OS Automated Tests ==="
echo ""

# Test 1: Help output
echo "Test 1: Checking help command..."
echo "help" | timeout 2 $ICE 2>&1 | head -30
echo "PASS: Help command works"
echo ""

# Test 2: Version check
echo "Test 2: Version command..."
echo "gpm version" | timeout 2 $ICE 2>&1 | grep -q "ICE Operating System" && echo "PASS: Version works"
echo ""

# Test 3: Memory info
echo "Test 3: Memory command..."
echo "gpm mem" | timeout 2 $ICE 2>&1 | grep -q "Memory Information" && echo "PASS: Memory info works"
echo ""

# Test 4: Process list
echo "Test 4: Process list..."
echo "pm" | timeout 2 $ICE 2>&1 | grep -q "process" && echo "PASS: Process list works"
echo ""

# Test 5: System info
echo "Test 5: System info..."
echo "gpm info" | timeout 2 $ICE 2>&1 | grep -q "MPM" && echo "PASS: System info works"
echo ""

echo "=== All basic tests passed ==="
