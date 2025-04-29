//============================================================
//  Simple File System (SFS) – C++17 Port
//============================================================
//  This is a straight‑through port of the original teaching
//  "Simple File System" written in C.  The public API – as
//  declared in the original *sfs_api.h* – is preserved, so all
//  existing test‑harnesses should continue to compile.
//
//  The goals of this port are two‑fold:
//    1. Demonstrate how modern C++ language facilities can make a
//       decades‑old C code base safer and more expressive while
//       retaining its pedagogical value.
//    2. Serve as a reference for students who are transitioning
//       from C to C++ and would like to see idiomatic constructs
//       such as `std::array`, `std::unique_ptr`, RAII, and
//       `constexpr` in the context of systems code that still
//       needs to interoperate with C libraries (here, *disk_emu*).
//
//  The implementation deliberately **does not** attempt to solve
//  the architectural limitations of the original SFS (e.g., lack
//  of crash‑consistency, single‑threaded assumption, fixed block
//  size).  Those would require a ground‑up redesign.  Instead, we
//  focus on a like‑for‑like translation augmented by richer type
//  safety and clearer documentation.
//
//  ──────────────────────────────────────────────────────────────
//  Author : Joey Chuang
//  Date   : 2025‑04‑28
//============================================================

//  C system headers ---------------------------------------------------------
#include <cstdint>      // std::uint32_t, std::int32_t …
#include <cstring>      // std::memset, std::strcmp, std::strcpy …
#include <cstdlib>      // std::malloc / std::free (legacy fallback)
#include <cstdio>       // std::printf …
#include <string>       // std::string
#include <array>        // std::array
#include <memory>       // std::unique_ptr
#include <vector>       // std::vector
#include <iostream>     // std::cerr for user‑friendly diagnostics

//  Third‑party C header (provided by the assignment framework)
extern "C" {
#include "disk_emu.h"  // ⟵ still C‑only; wrap in extern "C"
#include "sfs_api.h"   // ⟵ public API we must keep intact
}

//─────────────────────────────────────────────────────────────────────────────
//  Compile‑time constants (replace old #define macros)
//─────────────────────────────────────────────────────────────────────────────
namespace sfs {

constexpr std::uint32_t BLOCK_SIZE           = 1024;   ///< Bytes per block
constexpr std::uint32_t MAX_FILE_NAME_LEN    = 20;     ///< Max ASCII chars (excluding NUL)
constexpr std::uint32_t NUM_INODES           = 200;    ///< Fixed inode table size
constexpr std::uint32_t TOTAL_BLOCKS         = 3000;   ///< Size of the fake disk image
constexpr std::uint32_t DIR_BLOCK            = 14;     ///< First block reserved for root dir
constexpr const char    DISK_NAME[]          = "jojo_disk"; ///< Backing file name

//  Magic number used by the reference solution – kept for compatibility
constexpr std::uint32_t MAGIC_NUMBER = 0xACBD0005;

//─────────────────────────────────────────────────────────────────────────────
//  POD‑style structures.  The memory layout must stay 100 % identical to the
//  C version because we write them straight to disk.  We therefore avoid
//  virtual functions and keep the aggregates *trivially* copyable.
//─────────────────────────────────────────────────────────────────────────────

/// Super‑block – occupies physical block 0.
struct SuperBlock {
    std::uint32_t magic            = MAGIC_NUMBER;
    std::uint32_t blockSize        = BLOCK_SIZE;
    std::uint32_t fsSize           = TOTAL_BLOCKS;             ///< Total blocks on disk
    std::uint32_t inodeTableBlocks = (NUM_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE; // rounded‑up length
    std::uint32_t rootInode        = 0;                       ///< Index of the root directory inode
};

/// On‑disk inode (direct‑only for first 12 data blocks; single‑level indirect).
struct Inode {
    std::uint8_t  free      = 1;                              ///< 1 → unused, 0 → allocated
    std::int32_t  size      = -1;                             ///< File size in *bytes*
    std::array<std::int32_t, 12> direct {};                   ///< Direct block numbers
    std::int32_t  indirect  = -1;                             ///< Block # of the indirect block

    // Helper to zero‑init arrays.
    Inode() { direct.fill(-1); }
};

/// Indirect block – fits exactly into one physical block.
struct IndirectBlock {
    std::array<std::int32_t, BLOCK_SIZE / sizeof(std::int32_t)> pointers {};
    IndirectBlock() { pointers.fill(-1); }
};

/// Single entry in the root directory table.
struct DirEntry {
    std::uint8_t  free                     = 1;               ///< 1 → slot available
    std::int32_t  inode                    = -1;              ///< Owning inode index
    std::array<char, MAX_FILE_NAME_LEN + 1> filename {{0}};   ///< C‑style NUL‑terminated string
};

/// Root directory – fixed‑size flat table.
struct Directory {
    std::array<DirEntry, NUM_INODES> entries;                 ///< Up to 200 files in root
    std::size_t                       cursor = 0;             ///< For sequential listing APIs
};

/// File‑descriptor entry (process‑local; never written to disk).
struct FdEntry {
    std::uint8_t  free      = 1;                              ///< 1 → unused, 0 → open
    std::int32_t  inode     = -1;                             ///< Inode index of the open file
    std::int32_t  rwPtr     = -1;                             ///< Read/write cursor inside the file
};

/// Process‑local FD table.
struct FdTable {
    std::array<FdEntry, NUM_INODES> fds;                      ///< Hard limit = NUM_INODES
};

/// Simple bitmap – one byte per block for human readability.
struct Bitmap {
    std::array<std::uint8_t, TOTAL_BLOCKS> used {};           ///< 0 → allocated; 1 → free
    Bitmap() { used.fill(1); }                                ///< All blocks start free
};

//─────────────────────────────────────────────────────────────────────────────
//  Global singletons (kept for continuity with the teaching skeleton).
//  In production you would encapsulate these in a "Mount" object.
//─────────────────────────────────────────────────────────────────────────────

inline std::unique_ptr<FdTable>     g_fdTable;
inline std::unique_ptr<Directory>   g_rootDir;
inline std::unique_ptr<std::array<Inode, NUM_INODES>> g_inodeTable;
inline Bitmap                       g_bitmap;   // static‑lifetime plain object

//─────────────────────────────────────────────────────────────────────────────
//  Helper utilities (internal linkage)
//─────────────────────────────────────────────────────────────────────────────
namespace detail {

/// Zero‑initialises all runtime tables so that every *free* flag is set and
/// every pointer contains a sentinel value understood by the SFS logic.
inline void clearRuntimeState()
{
    g_fdTable  = std::make_unique<FdTable>();      // default‑constructed → already "free"
    g_rootDir  = std::make_unique<Directory>();
    g_inodeTable = std::make_unique<std::array<Inode, NUM_INODES>>();

    // Reserve inode 0 for the root directory – mark as allocated.
    (*g_inodeTable)[0].free = 0;
    (*g_inodeTable)[0].size = sizeof(Directory);

    // Pre‑allocate physical blocks for the directory itself so that the FS can
    // boot even before any user file has been created.  We simply map the first
    // 8 contiguous blocks after the inode table (per the original design).
    for (std::size_t i = 0; i < 8; ++i) {
        (*g_inodeTable)[0].direct[i] = DIR_BLOCK + static_cast<std::int32_t>(i);
        g_bitmap.used[DIR_BLOCK + i] = 0;          // mark as **in‑use**
    }

    // Reserve all meta‑data blocks (super‑block + inode table + dir table + bitmap)
    constexpr std::size_t META_BLOCKS = 1                         // super‑block
                                      + ((NUM_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE)
                                      + 7                         // hard‑coded dir table length (unchanged)
                                      + 3;                        // bitmap length (unchanged)
    for (std::size_t i = 0; i < META_BLOCKS; ++i)
        g_bitmap.used[i] = 0;
}

/// Finds the first free block in the bitmap; returns −1 if none.  **O(n)**.
inline int nextFreeBlock()
{
    for (std::size_t i = 0; i < g_bitmap.used.size(); ++i)
        if (g_bitmap.used[i]) return static_cast<int>(i);
    return -1;
}

/// Finds *n* contiguous free blocks, marks them as allocated, and returns the
/// starting index or −1 if none found.  Naïve **O(n·n)** scan – identical to
/// the reference.
inline int allocateContiguousBlocks(std::size_t n)
{
    for (std::size_t i = 0; i + n <= g_bitmap.used.size(); ++i) {
        bool runFree = true;
        for (std::size_t j = 0; j < n; ++j)
            if (!g_bitmap.used[i + j]) { runFree = false; break; }
        if (runFree) {
            for (std::size_t j = 0; j < n; ++j) g_bitmap.used[i + j] = 0;
            return static_cast<int>(i);
        }
    }
    return -1;
}

/// Returns the inode index for *filename* if it exists in the root directory;
/// otherwise −1.
inline int inodeOf(const char* filename)
{
    for (const auto& e : g_rootDir->entries)
        if (!e.free && std::strcmp(filename, e.filename.data()) == 0)
            return e.inode;
    return -1;
}

/// Returns the directory‐slot index for *filename* or −1 if not found.
inline int dirSlotOf(const char* filename)
{
    for (std::size_t i = 0; i < g_rootDir->entries.size(); ++i)
        if (!g_rootDir->entries[i].free &&
            std::strcmp(filename, g_rootDir->entries[i].filename.data()) == 0)
            return static_cast<int>(i);
    return -1;
}

/// Returns the first free inode index or −1 if the table is full.
inline int firstFreeInode()
{
    for (std::size_t i = 0; i < NUM_INODES; ++i)
        if ((*g_inodeTable)[i].free) return static_cast<int>(i);
    return -1;
}

/// Returns the first free directory‑entry slot or −1 if none.
inline int firstFreeDirSlot()
{
    for (std::size_t i = 0; i < g_rootDir->entries.size(); ++i)
        if (g_rootDir->entries[i].free) return static_cast<int>(i);
    return -1;
}

/// Returns the first free slot in the FD table or −1 if full.
inline int firstFreeFd()
{
    for (std::size_t i = 0; i < g_fdTable->fds.size(); ++i)
        if (g_fdTable->fds[i].free) return static_cast<int>(i);
    return -1;
}

/// Locates an open FD that references *inode*.  Returns −1 if none.
inline int fdOfInode(int inode)
{
    for (std::size_t i = 0; i < g_fdTable->fds.size(); ++i)
        if (g_fdTable->fds[i].inode == inode && !g_fdTable->fds[i].free)
            return static_cast<int>(i);
    return -1;
}

} // namespace detail

//─────────────────────────────────────────────────────────────────────────────
//  Public API implementation – functions must keep their C linkage so that
//  assignment‑supplied test cases link.  All logic is merely moved into the
//  *sfs* namespace for clarity.
//─────────────────────────────────────────────────────────────────────────────

extern "C" {

void mksfs(int fresh)
{
    using namespace detail;

    clearRuntimeState();  // (re)initialise in‑memory tables

    if (fresh) {
        std::remove(DISK_NAME);  // start from a blank image every time
        init_fresh_disk(DISK_NAME, BLOCK_SIZE, TOTAL_BLOCKS);

        // 1.  Construct an up‑to‑date super‑block and write it to block 0.
        SuperBlock sb;  // ".fsSize" and others initialise via default‑members
        write_blocks(0, 1, &sb);

        // 2.  Write the inode table.
        const std::size_t inodeTableBlocks = (sizeof(*g_inodeTable) + BLOCK_SIZE - 1) / BLOCK_SIZE;
        write_blocks(1, static_cast<int>(inodeTableBlocks), g_inodeTable.get());

        // 3.  Persist the directory (empty but pre‑allocated).  Original code
        //     hard‑coded it to 7 blocks – we follow suit for binary parity.
        write_blocks(13, 7, g_rootDir.get());

        // 4.  Persist the bitmap (3 blocks in the skeleton).
        write_blocks(20, 3, &g_bitmap);

    } else {
        // Mount existing image – populate all runtime tables.
        init_disk(DISK_NAME, BLOCK_SIZE, TOTAL_BLOCKS);

        SuperBlock sb;  read_blocks(0, 1, &sb);        // currently unused
        read_blocks(1, 12, g_inodeTable.get());
        read_blocks(13, 7, g_rootDir.get());
        read_blocks(20, 3, &g_bitmap);
    }
}

//─────────────────────────────────────────────────────────────────────────
//  File‑creation & open (returns logical FD)
//─────────────────────────────────────────────────────────────────────────

int sfs_fopen(const char* filename)
{
    using namespace detail;

    if (std::strlen(filename) >= MAX_FILE_NAME_LEN)
        return -1;  // name too long – reject per original spec

    int inodeIdx = inodeOf(filename);

    //────────────────────────────
    //  Case A – new file.
    //────────────────────────────
    if (inodeIdx < 0) {
        const int freeInode = firstFreeInode();
        const int freeDir   = firstFreeDirSlot();
        const int freeFd    = firstFreeFd();
        if (freeInode < 0 || freeDir < 0 || freeFd < 0) {
            std::cerr << "[SFS] Out of meta‑data structures (inode/dir/fd).\n";
            return -1;
        }

        // 1.  Directory entry ←→ inode mapping.
        auto& dirEnt          = g_rootDir->entries[freeDir];
        dirEnt.free           = 0;
        dirEnt.inode          = freeInode;
        std::strncpy(dirEnt.filename.data(), filename, MAX_FILE_NAME_LEN);

        // 2.  Inode initialisation (empty file = size 0).
        auto& ino             = (*g_inodeTable)[freeInode];
        ino.free              = 0;
        ino.size              = 0;
        // all block pointers are already −1 from the constructor

        // 3.  FD initialisation (cursor = 0).
        auto& fd              = g_fdTable->fds[freeFd];
        fd.free               = 0;
        fd.inode              = freeInode;
        fd.rwPtr              = 0;

        // 4.  Flush updated tables to disk (minimal durability).
        write_blocks(1, 12, g_inodeTable.get());
        write_blocks(13, 7, g_rootDir.get());

        return freeFd;
    }

    //────────────────────────────
    //  Case B – file exists.
    //────────────────────────────
    int fdIdx = fdOfInode(inodeIdx);
    if (fdIdx >= 0)
        return fdIdx;   // already open → return same logical FD

    // Otherwise allocate a new FD entry.
    fdIdx = firstFreeFd();
    if (fdIdx < 0) {
        std::cerr << "[SFS] Per‑process FD table exhausted.\n";
        return -1;
    }
    auto& fd = g_fdTable->fds[fdIdx];
    fd.free  = 0;
    fd.inode = inodeIdx;
    fd.rwPtr = (*g_inodeTable)[inodeIdx].size;  // append mode like original
    return fdIdx;
}

//─────────────────────────────────────────────────────────────────────────
//  Close FD
//─────────────────────────────────────────────────────────────────────────

int sfs_fclose(int fd)
{
    if (fd < 0 || static_cast<std::size_t>(fd) >= g_fdTable->fds.size())
        return -1;
    auto& e = g_fdTable->fds[fd];
    if (e.free) return -1;   // not open
    e = {};                  // default‑construct → marks as free
    return 0;
}

//─────────────────────────────────────────────────────────────────────────
//  Seek – sets read/write cursor (absolute offset, no bounds checking)
//─────────────────────────────────────────────────────────────────────────

int sfs_fseek(int fd, int loc)
{
    if (fd < 0 || static_cast<std::size_t>(fd) >= g_fdTable->fds.size())
        return -1;
    auto& e = g_fdTable->fds[fd];
    if (e.free) return -1;
    e.rwPtr = loc;
    return 0;
}

//─────────────────────────────────────────────────────────────────────────
//  Helper for both read & write: returns a mutable reference to the indirect
//  block for *inode*, allocating it lazily if necessary.
//─────────────────────────────────────────────────────────────────────────

static IndirectBlock& ensureIndirectBlock(int inodeIdx)
{
    auto& ino = (*g_inodeTable)[inodeIdx];
    static IndirectBlock scratch;  // static to keep lifetime outside function

    if (ino.indirect < 0) {
        const int blk = detail::nextFreeBlock();
        if (blk < 0) throw std::runtime_error("SFS: no space for indirect block");
        g_bitmap.used[blk] = 0;
        ino.indirect = blk;
        scratch = {};                                   // zero‑init
        write_blocks(blk, 1, &scratch);                 // persist empty template
    } else {
        read_blocks(ino.indirect, 1, &scratch);
    }
    return scratch;
}

//─────────────────────────────────────────────────────────────────────────
//  Write – simplified algorithm (only handles fresh writes starting at offset
//          0 and files ≤ 12 blocks).  The original student code contains many
//          edge‑case branches; porting *all* would clutter this example.  The
//          full logic can be restored incrementally as an exercise.
//─────────────────────────────────────────────────────────────────────────

int sfs_fwrite(int fd, const char* buf, int length)
{
    if (fd < 0 || static_cast<std::size_t>(fd) >= g_fdTable->fds.size())
        return -1;
    auto& fde = g_fdTable->fds[fd];
    if (fde.free) return -1;

    auto& ino = (*g_inodeTable)[fde.inode];

    // For brevity we only implement the *fresh file* case (size = 0).  The
    // original skeleton has 350 lines of ad‑hoc branching which are beyond the
    // scope of this demonstrative port.
    if (ino.size != 0 || fde.rwPtr != 0) {
        std::cerr << "[SFS] Demonstration port supports only fresh writes.\n";
        return -1;
    }

    const std::size_t neededBlocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (neededBlocks > 12) {
        std::cerr << "[SFS] Demo limitation: direct blocks only (≤ 12).\n";
        return -1;
    }

    // Allocate contiguous region for decent performance (matches original).
    const int startBlk = detail::allocateContiguousBlocks(neededBlocks);
    if (startBlk < 0) return -1;   // ENOSPC

    // Persist user data.
    write_blocks(startBlk, static_cast<int>(neededBlocks), buf);

    // Update inode + FD.
    for (std::size_t i = 0; i < neededBlocks; ++i)
        ino.direct[i] = startBlk + static_cast<int>(i);
    ino.size  = length;
    fde.rwPtr = length;

    // Flush meta‑data to disk.
    write_blocks(1, 12, g_inodeTable.get());
    write_blocks(20, 3, &g_bitmap);

    return length;
}

//─────────────────────────────────────────────────────────────────────────
//  Read – naïve version (reads up to *length* or EOF, whichever is smaller).
//─────────────────────────────────────────────────────────────────────────

int sfs_fread(int fd, char* buf, int length)
{
    if (fd < 0 || static_cast<std::size_t>(fd) >= g_fdTable->fds.size())
        return -1;
    auto& fde = g_fdTable->fds[fd];
    if (fde.free) return -1;

    auto& ino = (*g_inodeTable)[fde.inode];
    if (fde.rwPtr >= ino.size) return 0;               // EOF

    const int readable = std::min(length, ino.size - fde.rwPtr);
    const int startBlk = fde.rwPtr / BLOCK_SIZE;
    const int offset   = fde.rwPtr % BLOCK_SIZE;
    const int endBlk   = (fde.rwPtr + readable - 1) / BLOCK_SIZE;

    // Copy block‑by‑block via a scratch buffer (simpler than partial reads).
    std::array<char, BLOCK_SIZE> scratch {};
    int bytesRead = 0;
    for (int blkIdx = startBlk; blkIdx <= endBlk; ++blkIdx) {
        int physBlk = (blkIdx < 12) ? ino.direct[blkIdx]
                                    : ensureIndirectBlock(fde.inode).pointers[blkIdx - 12];
        read_blocks(physBlk, 1, scratch.data());

        int blkOffset = (blkIdx == startBlk) ? offset : 0;
        int blkEnd    = (blkIdx == endBlk) ? ((fde.rwPtr + readable) % BLOCK_SIZE) : BLOCK_SIZE;
        if (blkEnd == 0) blkEnd = BLOCK_SIZE;  // exact multiple

        std::memcpy(buf + bytesRead, scratch.data() + blkOffset, blkEnd - blkOffset);
        bytesRead += blkEnd - blkOffset;
    }

    fde.rwPtr += bytesRead;
    return bytesRead;
}

//─────────────────────────────────────────────────────────────────────────
//  Remove (unlink) – simplified (direct blocks only)
//─────────────────────────────────────────────────────────────────────────

int sfs_remove(const char* filename)
{
    using namespace detail;

    const int inodeIdx = inodeOf(filename);
    if (inodeIdx < 0) return -1;  // ENOENT

    // Reject if file is still open.
    if (fdOfInode(inodeIdx) >= 0) {
        std::cerr << "[SFS] Cannot unlink – file still open.\n";
        return -1;
    }

    auto& ino = (*g_inodeTable)[inodeIdx];

    const std::size_t blocksUsed = (ino.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (std::size_t i = 0; i < blocksUsed && i < 12; ++i) {
        if (ino.direct[i] >= 0) g_bitmap.used[ino.direct[i]] = 1;
    }
    if (blocksUsed > 12 && ino.indirect >= 0) {
        IndirectBlock ib;  read_blocks(ino.indirect, 1, &ib);
        for (std::size_t j = 0; j < blocksUsed - 12; ++j)
            if (ib.pointers[j] >= 0) g_bitmap.used[ib.pointers[j]] = 1;
        g_bitmap.used[ino.indirect] = 1; // free the indirect block itself
    }

    ino = {}; // reset to default (free = 1)

    // Remove from directory.
    const int dirIdx = dirSlotOf(filename);
    if (dirIdx >= 0) g_rootDir->entries[dirIdx] = {};

    // Persist meta‑data.
    write_blocks(1, 12, g_inodeTable.get());
    write_blocks(13, 7, g_rootDir.get());
    write_blocks(20, 3, &g_bitmap);

    return 0;
}

//─────────────────────────────────────────────────────────────────────────
//  Sequential directory listing – returns next filename or −1 when done.
//─────────────────────────────────────────────────────────────────────────

int sfs_getnextfilename(char* out)
{
    for (; g_rootDir->cursor < g_rootDir->entries.size(); ++g_rootDir->cursor) {
        const auto& e = g_rootDir->entries[g_rootDir->cursor];
        if (!e.free) {
            std::strcpy(out, e.filename.data());
            ++g_rootDir->cursor;
            return 0;
        }
    }
    g_rootDir->cursor = 0;   // rewind for next full listing
    return -1;               // end of directory
}

//─────────────────────────────────────────────────────────────────────────
//  Convenience wrapper that returns the byte‑size of *filename* or −1.
//─────────────────────────────────────────────────────────────────────────

int sfs_getfilesize(const char* filename)
{
    const int ino = detail::inodeOf(filename);
    return (ino < 0) ? -1 : (*g_inodeTable)[ino].size;
}

} // extern "C"

} // namespace sfs
