"""DataReel for Python (7.5 beta 2): read and write the object databases the DataReel C++ library writes.

Primary(root, config=None, settings=None, quiet=False)
    7.5 beta 2: a gxObjRepl primary node, written from Python: open_database(db), begin(), new_id(),
    put(db, id, data), remove(db, id), purge(db, id), commit(), rollback(), with transaction(): ...,
    pump(), drain(seconds); and its reads (get, is_live, ids, databases, find_key, nearest); last_seq,
    epoch, copy_applied. One process holds a node: a primary written from Python is that process.
Reader(root, index="")
    A gxObjRepl node folder, read only, while the node runs in another process (or no node runs).
    Each call sees whole commits only: get(db, id), is_live(db, id), ids(db, tombstones=False),
    databases(), find_key(index, key), nearest(index, query, k=10), refresh(); seq, epoch, role.
VectorIndex(path, spec=None)
    A .vix vector index file, read only (a file no process is writing): search(query, k=10, ef=64,
    exact=False, tags=None), get(id); dim, count, spec, metric.
BlockFile(path)
    A .gxd block file, read only (a file no process is writing): addresses(), read(address).
status(root), digest(root, links=""), takeover(root)
    7.5 beta 2, node folder tools (no process holds ROOT): the node's state as a dict; its content
    digest (equal on two nodes with the same objects); a stopped copy made a primary at the next epoch
    (for a standalone test node made from a copy: it never follows its old primary again).
Error
    Raised when DataReel cannot do what was asked (the message says why).

Objects are bytes, exactly as the C++ programs store them: encode and decode them with struct or
numpy using the record layout the C++ programs use. The 64-bit database engine only.
"""
import contextlib

from ._datareel import (Error, Reader, VectorIndex, BlockFile, library_version, engine_bits, _Primary,
                        status, digest, takeover)

__version__ = "7.5b2"
__all__ = ["Error", "Primary", "Reader", "VectorIndex", "BlockFile", "library_version", "engine_bits",
           "status", "digest", "takeover", "__version__"]


class Primary(_Primary):
    """A gxObjRepl primary node written from Python (DataReel 7.5 beta 2).

    Primary(root, config=None, settings=None, quiet=False): ROOT the node folder (made if new); CONFIG an
    objrepl.conf file and SETTINGS a dict of the same keys ("copy", "ca", "cert", "key", "index", "links",
    "name", "peer-name", "trim", "idle-timeout", "ack-timeout" ...), which win over the file. QUIET drops
    the node's messages (by default they go to stderr).
    """

    @contextlib.contextmanager
    def transaction(self):
        """with p.transaction(): ... -- begin(), then commit() when the block ends, or rollback() when it
        raises (the exception goes on)."""
        self.begin()
        try:
            yield self
        except BaseException:
            self.rollback()
            raise
        self.commit()
