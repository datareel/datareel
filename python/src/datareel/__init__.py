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
Copy(root, config=None, settings=None, quiet=False)
    7.5 beta 2: a gxObjRepl copy node in this process: serve() (blocks; run it in a thread) applies what
    the primary sends until stop(); reads as the primary's; applied_seq, epoch, in_snapshot.
status(root), digest(root, links=""), takeover(root), rejoin(root), check(root, settings=None, config=None),
clear_alarm(root), index_check(root, index)
    7.5 beta 2, node folder tools (no process holds ROOT): the node's state as a dict; its content
    digest (equal on two nodes with the same objects); a stopped copy made a primary at the next epoch
    (for a standalone test node made from a copy: it never follows its old primary again); a former
    primary made a copy waiting for a full send; OK/WARN/FAIL from the status and alarm files; the alarm
    set aside; each index checked against the objects.
Database(path, create=False, readonly=False), BTree(path, key_size, create=False, order=16, readonly=False),
Journal(path, write_back=False), create_vector_index(...), merge_vector_indexes(...)
    7.5 beta 2, the database layer: gxDatabase block files (alloc, write, read, delete), gxBtree index
    files of fixed-size byte keys with an 8-byte value (insert, delete, find, prefix, range), vector
    index writes (VectorIndex(path, write=True): insert, update, delete, repair, verify), and gxdJournal
    transactions over any of them (attach, begin, commit, rollback; transaction(journal)).
Error
    Raised when DataReel cannot do what was asked (the message says why).

Objects are bytes, exactly as the C++ programs store them: encode and decode them with struct or
numpy using the record layout the C++ programs use. The 64-bit database engine only.
"""
import contextlib

from ._datareel import (Error, Reader, VectorIndex, BlockFile, Copy, Database, BTree, Journal, library_version,
                        engine_bits, _Primary, status, digest, takeover, rejoin, check, clear_alarm, index_check,
                        create_vector_index, merge_vector_indexes)

__version__ = "7.5b2"
__all__ = ["Error", "Primary", "Copy", "Reader", "VectorIndex", "BlockFile", "Database", "BTree", "Journal",
           "library_version", "engine_bits", "status", "digest", "takeover", "rejoin", "check", "clear_alarm",
           "index_check", "create_vector_index", "merge_vector_indexes", "transaction", "__version__"]


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


@contextlib.contextmanager
def transaction(journal):
    """with datareel.transaction(j): ... -- j.begin(), then j.commit() when the block ends, or j.rollback() when
    it raises (the exception goes on)."""
    journal.begin()
    try:
        yield journal
    except BaseException:
        journal.rollback()
        raise
    journal.commit()
