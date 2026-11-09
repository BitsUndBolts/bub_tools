# bub_tools

A collection of small, useful tools for working with old retro hardware, written over time
for the **Bits und Bolts** channel, by Alex or together with Claude.

Repairing and testing vintage PCs keeps raising the same little problems: copying a floppy
exactly, checking a card, poking at a port. Instead of rewriting a helper each time, it is
kept here, documented and ready to use again. Many tools target MS-DOS, but anything that
helps on the bench belongs here.

## Tools

| Folder | Tool | Description |
|--------|------|-------------|
| [flopimg](flopimg/) | FLOPIMG | Floppy imaging for DOS: read, write, verify and repair bootable disks |

## How the repository is organized

Each tool lives in its own lowercase folder and has its own `README.md` with usage, build
instructions and any warnings. Start there.

```
bub_tools/
  README.md        this overview
  <tool>/
    README.md      what the tool does, how to use and build it
    <source>       source code
    bin/           ready-to-use binary
```

Conventions:
- The built binary is committed in `bin/`, because the retro PCs usually have no toolchain.
- DOS tools use 8.3 file names so they work unchanged on the DOS PC.
- A tool that can overwrite data asks for confirmation first and offers a read-only mode
  (verify / dry run) where that makes sense.

## Adding a tool

1. Create `<tool>/` with the source, a `README.md` and, after building, `bin/`.
2. Add a row to the table above.
3. Commit source and binary together.
