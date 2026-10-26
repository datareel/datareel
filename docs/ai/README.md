# DataReel 7 guides for AI coding assistants

Short, task-oriented descriptions of the DataReel 7 API with complete
programs. Every program here is compiled and run by
`tests/run_tests.sh`, so the code matches the library.

Start with `AGENTS.md` in the top folder for the rules, then:

- [TLS communications](tls-comms.md): TLS clients and servers, HTTPS,
  SMTP, POP3, FTPS, gxStream.
- [Object database](object-database.md): block files, B-tree indexes,
  POD, InfoHog persistent objects, gxRDBMS tables, file repair.
- [Building and linking](building.md): CMake options, file format
  settings, using DataReel from your project, tests.
- [Porting from 6.x](porting-from-6x.md): replacements for removed
  classes and the changes to network code.

`samples/` holds the programs the guides include. After changing one,
run `python3 docs/bin/make_ai_docs.py` to refresh the guides,
`llms.txt` and `llms-full.txt`.
