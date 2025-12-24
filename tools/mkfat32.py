#!/usr/bin/env python3
import sys
import struct

def create_fat32_image(filename, size_mb=32):
    sector_size = 512
    total_sectors = (size_mb * 1024 * 1024) // sector_size
    reserved_sectors = 32
    sectors_per_cluster = 1
    num_fats = 2
    root_cluster = 2
    
    # Calculate FAT size
    # Total clusters ~= total_sectors / sectors_per_cluster
    # Each FAT entry is 4 bytes.
    # fat_size_bytes = total_clusters * 4
    # fat_size_sectors = fat_size_bytes / 512
    total_clusters = total_sectors // sectors_per_cluster
    fat_size_sectors = (total_clusters * 4 + 511) // 512
    
    # BPB Structure (FAT32) - 90 bytes usually, but we need to fill the boot sector
    # jmp(3), oem(8), bps(2), spc(1), res(2), fats(1), root_ent(2), tot16(2), media(1), fat16(2)
    # spt(2), heads(2), hidden(4), tot32(4), fat32(4), ext(2), ver(2), root(4), info(2), bk(2)
    # reserved(12), drive(1), res(1), sig(1), volid(4), label(11), type(8)
    
    bpb = bytearray(512)
    
    # Jump instructions (EB 3C 90)
    struct.pack_into('<3B', bpb, 0, 0xEB, 0x3C, 0x90)
    
    # OEM Name
    struct.pack_into('<8s', bpb, 3, b'MKFAT32 ')
    
    # BIOS Parameter Block
    struct.pack_into('<H', bpb, 11, sector_size)       # Bytes per sector
    struct.pack_into('<B', bpb, 13, sectors_per_cluster) # Sectors per cluster
    struct.pack_into('<H', bpb, 14, reserved_sectors)  # Reserved sectors
    struct.pack_into('<B', bpb, 16, num_fats)          # Number of FATs
    struct.pack_into('<H', bpb, 17, 0)                 # Root entries (0 for FAT32)
    struct.pack_into('<H', bpb, 19, 0)                 # Total sectors 16 (0 for FAT32)
    struct.pack_into('<B', bpb, 21, 0xF8)              # Media descriptor (Fixed disk)
    struct.pack_into('<H', bpb, 22, 0)                 # FAT size 16 (0 for FAT32)
    struct.pack_into('<H', bpb, 24, 32)                # Sectors per track
    struct.pack_into('<H', bpb, 26, 64)                # Number of heads
    struct.pack_into('<I', bpb, 28, 0)                 # Hidden sectors
    struct.pack_into('<I', bpb, 32, total_sectors)     # Total sectors 32
    
    # FAT32 Extended
    struct.pack_into('<I', bpb, 36, fat_size_sectors)  # FAT size 32
    struct.pack_into('<H', bpb, 40, 0)                 # Ext flags
    struct.pack_into('<H', bpb, 42, 0)                 # FS version
    struct.pack_into('<I', bpb, 44, root_cluster)      # Root cluster
    struct.pack_into('<H', bpb, 48, 1)                 # FS Info sector
    struct.pack_into('<H', bpb, 50, 6)                 # Backup boot sector
    
    # Drive info
    struct.pack_into('<B', bpb, 64, 0x80)              # Drive number
    struct.pack_into('<B', bpb, 66, 0x29)              # Ext boot signature
    struct.pack_into('<I', bpb, 67, 0x12345678)        # Volume ID
    struct.pack_into('<11s', bpb, 71, b'ICEOS DISK ')  # Volume Label
    struct.pack_into('<8s', bpb, 82, b'FAT32   ')      # FS Type
    
    # Boot signature len 2 at offset 510
    struct.pack_into('<H', bpb, 510, 0xAA55)
    
    # Create file
    with open(filename, 'wb') as f:
        # Write Boot Sector
        f.write(bpb)
        
        # Fill Reserved Sectors (minus boot sector)
        f.write(b'\x00' * (sector_size * (reserved_sectors - 1)))
        
        # Write FAT 1
        # Entry 0: Media type | FFFFF00
        # Entry 1: EOC
        # Entry 2: EOC (Root dir, empty logic usually uses EOC)
        fat_data = bytearray(sector_size * fat_size_sectors)
        struct.pack_into('<I', fat_data, 0, 0x0FFFFFF8) # Entry 0
        struct.pack_into('<I', fat_data, 4, 0x0FFFFFFF) # Entry 1
        struct.pack_into('<I', fat_data, 8, 0x0FFFFFFF) # Entry 2 (Root)
        f.write(fat_data)
        
        # Write FAT 2
        f.write(fat_data)
        
        # Write Data Area (Clusters)
        # We need to fill up to total_sectors
        # Sectors used so far: reserved + (num_fats * fat_size)
        used_sectors = reserved_sectors + (num_fats * fat_size_sectors)
        remaining_sectors = total_sectors - used_sectors
        
        # Initialize Root Directory (Cluster 2)
        # Root is one cluster long initially (all zeros = empty)
        # Cluster 2 is the FIRST cluster in data area.
        f.write(b'\x00' * (sector_size * sectors_per_cluster))
        
        # Fill remaining disk with zero (sparse if OS supports it, but here we write)
        # Writing 32MB of zeros is fast enough.
        # Actually, seek to end to make it sparse
        f.seek((total_sectors * sector_size) - 1)
        f.write(b'\x00')

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: mkfat32.py <output_file>")
        sys.exit(1)
    create_fat32_image(sys.argv[1])
