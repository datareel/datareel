"""DataReel for Python (7.5 beta): read the object databases the DataReel C++ library writes.

Reader(root, index="")
    A gxObjRepl node folder, read only, while the node runs in another process (or no node runs).
    Each call sees whole commits only: get(db, id), is_live(db, id), ids(db, tombstones=False),
    databases(), find_key(index, key), nearest(index, query, k=10), refresh(); seq, epoch, role.
VectorIndex(path, spec=None)
    A .vix vector index file, read only (a file no process is writing): search(query, k=10, ef=64,
    exact=False, tags=None), get(id); dim, count, spec, metric.
BlockFile(path)
    A .gxd block file, read only (a file no process is writing): addresses(), read(address).
Error
    Raised when DataReel cannot do what was asked (the message says why).

Objects come back as bytes, exactly as the C++ program stored them: decode them with struct or
numpy using the program's record layout. The 64-bit database engine only.
"""
from ._datareel import Error, Reader, VectorIndex, BlockFile, library_version, engine_bits

__version__ = "7.5b1"
__all__ = ["Error", "Reader", "VectorIndex", "BlockFile", "library_version", "engine_bits", "__version__"]
