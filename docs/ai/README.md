# DataReel 7 guides for AI coding assistants

Short, task-oriented descriptions of the DataReel 7 API with complete
programs. Every program here is compiled and run by
`tests/run_tests.sh`, so the code matches the library.

AI-assisted use only: before you use any AI assistance, review your
agency's or company's AI usage policies and get formal written approval
from your management team, use only approved enterprise AI models, and
keep a person in the loop for every step. See "Before you use AI
assistance" in `AGENTS.md` and https://datareel.com/terms.php#ai-assisted.
Naming an AI tool is a reference only, not an endorsement.

Start with `AGENTS.md` in the top folder for the rules, setup, builds,
upgrades, forks and support, then:

- [TLS communications](tls-comms.md): TLS clients and servers, HTTPS,
  SMTP, POP3, FTPS, gxStream.
- [Object database](object-database.md): block files, B-tree indexes,
  POD, InfoHog persistent objects, file repair.
- [Building and linking](building.md): CMake options, file format
  settings, using DataReel from your project, tests.
- [Porting from 6.x](porting-from-6x.md): replacements for removed
  classes and the changes to network code.
- [Text, CSV, single-instance and file utilities](utilities.md):
  strip_ansi/AnsiStripper, gxCSVReader/gxCSVWriter, gxProcessLock,
  hexdump, ffind, txfilter (7.4).
- [Troubleshooting and support](troubleshooting-and-support.md):
  read-only checks, problems by area, when to stop, the support summary.

`samples/` holds the programs the guides include. After changing one,
run `python3 docs/bin/make_ai_docs.py` to refresh the guides,
`llms.txt` and `llms-full.txt`.
