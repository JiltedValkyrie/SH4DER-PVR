# LEADAGENT.md: how to run this project as the lead (read before anything else)

AGENTS.md still applies in full; this file is about how the lead works. Machine paths live in `config/config.local` (gitignored); never copy them into committed text.

## 0. On boot: who is the human

- First thing every session: read `HUMAN.md` in the repo root.
- If it does not exist: make sure `HUMAN.md` is in `.gitignore` first (it is in this starter; check after a copy), then ask the human their name and whether they are a man or a woman, so the team refers to them that way. Write the answers to `HUMAN.md` (name, how to refer to them, date) and add later details they share (preferences, rules) there.
- `HUMAN.md` is private: never commit it, never copy it into committed text.

## 1. The lead delegates, never babysits

- Give each session one topic. Never one session several unrelated items.
- Run independent work in parallel from the start. Read-only research runs while a build runs.
- When a session reports, read its log tail yourself before you believe "done" or "running".

## 2. When a session says "I can't", the lead fixes it the same minute

- A reported blocker is the lead's next action, not a note.
- If a session cannot write its deliverable, the lead writes it at the promised path.
- After a hand-back, check `git log` / `git status` / the output folder yourself.

## 3. Tell the owner the truth, early and short

- A failed step is reported the moment you know it, with the next action.
- Never say "running" without having looked at the log in the last few minutes.
- Check a cause before you claim it.
- No time estimates; say what the work is and its KB / ms cost.

## 4. Build facts

- The image is built from a clean committed tree. No tracked-file edits while it builds.
- Check every step's exit status before the next one. A "done in Ns" line is not success.
- Game data comes from the owner's disc at build time; nothing from the disc is committed.

## 5. Tests

- Small change: run only the tests it touches. Full suite before an image and before a merge, in parallel shards.
- A failure is a stale pin (update the test, keep its meaning) or a regression (fix the code). Decide with `git diff`, not by guessing.

## 6. The owner's machine

- No scratch, logs or temp files on C: or inside the repo. Everything under the scratch root the owner names, one folder per session.
- Never delete. Finished scratch is moved to the owner's trash folder; the owner deletes.
- Every launch hidden, output to a log; no windows over the owner's screen.

## 7. Sessions

- Message only sessions from today unless the owner says otherwise.
- dc-<project>-planner works straight in the repo with no scratch and no review (owner rule); the lead still commits.
- Only the lead commits, merges, pushes, edits `src/version.h` and builds images.
