# CLAUDE.md

The project knowledge base is **[AGENTS.md](AGENTS.md)**. Read it first. It covers the layout, every parameter, the engine invariants, the test setup, the decisions and their reasons, the open risks, and the JUCE and CMake gotchas.

This file holds only Claude Code specifics.

## Claude Code notes

- **Build tooling.** CMake is not on the bash PATH. Use the full path:
  `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
  (and `ctest.exe` in the same folder). See AGENTS.md section 3.
- **Files change on disk.** The IDE and the owner edit files between reads. The harness reports changes; take the on-disk state as current and re-read before editing.
- **Git.** Do not commit or push unless the owner asks.
- **Test expectations.** Run the full CTest suite before reporting a change as working. Editor tests need a desktop session, so a headless run can fail for reasons that are not code bugs.
- **Prefer the existing helpers.** `tests/common/PluginTestUtils.h` and the DSP tests already cover most patterns. Reuse them before writing new ones.

## Working notes for Claude Code

- The PowerShell working directory persists between calls in one session. Earlier a Bash \cd\ moved it into \docs/plans\, and a search then ran in the wrong folder. Use absolute paths in every command.
- In PowerShell, \ash\ starts WSL on this machine, not Git Bash. Use the Bash tool for \git archive\, \sed\ and \grep\.
- Edit files with the Edit tool. If a script must edit a UTF-8 file, write bytes and keep the BOM. \Set-Content\ writes ANSI and corrupts non-ASCII text.
- Auto mode refused a command that would stop running CMake processes. The IDE may own them. Do not look for another way to stop them; report what is running and let the owner decide.
- Keep build logs in \uild/logs/\, not in \%TEMP%\. Keep baselines and throwaway builds in \uild/\.
- Stop before \git commit\ and \git push\. The owner commits between steps.
