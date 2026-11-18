# Personal update channel

This is kevbin's personal Linux x86_64 fork. It keeps the Simplified Chinese interface and the GNOME scrolling-capture fix. It is not an official Unisic release.

`Fork updates` runs on the default `main` branch every six hours, at minute 23 UTC, and supports **Run workflow** in GitHub Actions. An unchanged maintained source commit does not produce another release. Enable `force_release` to rebuild that commit. Only stable upstream releases are merged; ordinary commits on upstream development branches do not trigger a release.

The prepare job fetches the upstream release tag and merges it without a rebase or force push. Files in `.github/workflows/` remain owned by this fork. Source conflicts, dirty checkouts, unsupported version formats, and missing Chinese translations stop the job. This intentionally leaves the previous published version available rather than silently discarding a patch or shipping untranslated new UI. Resolve conflicts/translate new messages on main and run the workflow again. Changes to upstream packaging dependencies may require manually updating the fork-owned workflows.

The exact prepared commit is built by the existing reusable QtTest and AppImage workflows. Python automation tests exercise merge preservation, conflict rollback, release deduplication and version validation. The Chinese catalog gate checks coverage and placeholders. Both workflows must pass, then the publish job checks the runnable AppImage's version and zsync metadata before publishing the draft release. The verification runner installs the PipeWire client and Wayland libraries supplied by a real desktop; these platform libraries are deliberately excluded from AppImage deployment. `Inspect AppImage` can diagnose an existing artifact and recover publication only after it verifies the original test/package jobs and exact runtime version. Failed builds never become the latest update. GitHub Actions failure notifications follow the repository owner's GitHub notification settings.

A fork version adds a fourth numeric component: upstream `0.8.9` plus workflow run 12 becomes `0.8.9.12`. Both the C++ version and release tag carry it. The existing `QVersionNumber` comparator orders fork revisions and subsequent upstream versions without suffix parsing. `UNISIC_FORK_VERSION` is provided only to personal release builds; the upstream CMake version declaration remains untouched for future merges. The official multi-platform release workflow runs only in `unisic/unisic`, preventing a second publisher on the fork.

Release assets include the AppImage, zsync metadata, portable tarball, SHA256SUMS and build-info.json with source/upstream commits and the workflow link. The built-in stable/beta feeds and AppImage differential-update metadata all point to `kevbin/unisic`. Never point this build at the official feed: doing so installs a version without these patches.

The app's existing update policy checks approximately 10 seconds after startup and once daily when automatic checks are enabled. On discovery it notifies and downloads, verifies GitHub's asset checksum, then stages or replaces the image. An idle restart activates it; active editing/recording delays restart. The current unpacked installation uses the existing staged-update bootstrap, so the menu launcher remains valid. A network failure only delays the check. No extra personal token or separate server is required: scheduled jobs use the repository-scoped GITHUB_TOKEN with write permission only for prepare/publish.

Checks:

```sh
python3 -m unittest discover -s tests -p test_fork_release.py -v
python3 scripts/fork_release.py --check-catalog
ctest --test-dir build --output-on-failure --no-tests=error
```

The sibling `unisic-website` repository is unavailable in this workspace. Its official installer still installs the official build and has not been changed to distribute this personal fork.
