# Contributing to Régie5

Thanks for your interest. Régie5 is a small project made for people who run their
own media server and have a jailbroken PS5: reports from real set-ups are what
make it better. This page says how to help, and which rules every change follows.

## Report a bug

Open an issue with the **Bug report** form. The most useful reports have:

- the Régie5 version (bottom of Settings, the gear) and the console's firmware;
- the services involved and their versions (Sonarr, Radarr, Seerr, Jellyfin or
  Emby, Glances), and whether they sit behind a reverse proxy or HTTPS;
- what you did, what you expected, what you saw: **a photo of the TV is perfect**;
- for a connection problem, the message on the service's line in Settings.

**Never paste an API key, a password or your `config.json` as it is.** Replace
keys with `xxx` and addresses with `192.168.x.x`.

Emby support is in beta: Emby reports go to
[issue #1](https://github.com/viviandsx/regie5/issues/1).

## Suggest an idea

Open an issue with the **Feature request** form. Say what you would do from the
couch and why it helps; a screenshot of the same thing elsewhere (the service's
web interface, another app) helps too. Read the scope below first: some ideas
cannot be accepted, however good.

## Translate

A language is a single file. [TRANSLATING.md](TRANSLATING.md) explains how to add
one and how to check it on screen without a console. If Spanish or German is
your language, corrections are very welcome (TRANSLATING.md says why).

## Change the code

- Régie5 is developed and tested on Linux, where the same app runs in a window
  (`scripts/run-linux.sh --demo` needs no server). The README's *Building from
  source* has the commands; `ctest --test-dir build/linux` runs the tests.
- Keep pull requests small and about one thing. Say how you tested it (Linux,
  console, which services and versions).
- A change that reads a new API needs a fixture: a real answer, **anonymised**
  (no real titles, names, addresses or keys), in `fixtures/`, and a test.
- Follow the code around it: names, comments, no new dependency without a word
  in the issue first.
- Most design notes in `docs/` are in French; questions in English are fine.
- By contributing, you agree that your work is published under the project's
  licence, the **GNU GPL v3.0 or later**.

Reviews, tests on other set-ups and plain human eyes on the code are welcome
(the README's *How Régie5 was made* says how it was written).

## The rules of the project

These hold for every feature, now and later. A pull request that breaks one is
closed, however well made.

**Never in Régie5:**

- a download client (qBittorrent, Transmission, SABnzbd…), Prowlarr or any other
  indexer;
- a search for releases, interactive or not, or any choice of release;
- the name of a release, a release group or a tracker, anywhere on screen (when
  an API gives it, Régie5 does not read it: it shows the title, the progress, the
  time left and the quality);
- a link to a download site.

This keeps Régie5 within the rules of [homebrew.page](https://homebrew.page),
which does not list tools whose main purpose is piracy.

**Always:**

- **Read-only by default.** Every action that changes something on a server is
  listed in the README (*What Régie5 changes on your servers*) and asks for
  confirmation, with the focus starting on **Cancel**.
- **Destructive features are off by default** and turned on by the user in
  Settings, after a clear warning (Cleanup).
- **Local network only.** Régie5 talks to the user's own servers, never to the
  internet (the console may have no internet at all): images come from the
  servers, never from outside.
- **Privacy.** No IP address or device name from Jellyfin's or Emby's activity
  log; API keys are never logged.
- **For everybody's set-up.** HTTPS, reverse-proxy sub-paths, several instances
  of a service, older versions with a clear message: never one person's
  installation assumed.
- **Made for a TV.** Large type readable from 3 m, everything reachable with the
  controller, nothing that needs a keyboard to set up.
