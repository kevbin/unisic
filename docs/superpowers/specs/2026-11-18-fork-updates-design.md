# Personal fork updates

The user approved automatic upstream synchronization, preserved Simplified Chinese and GNOME scrolling-capture fixes, cloud compilation, and updates from kevbin/unisic.

Every six hours a workflow on the fork's default main branch checks the latest upstream stable release. It merges that release into main without rebasing or force-pushing. The fork owns its workflow files; upstream source conflicts or incomplete Chinese translations stop the run. The last working published release stays available after failures.

The existing QtTest workflow and AppImage workflow build the exact prepared commit. Both must succeed before publication. Fork versions append a numeric fourth component to the upstream version, using the workflow run number (for example 0.8.9.1), so the existing version comparator detects both fork revisions and newer upstream releases. The immutable tag and binary use the same version.

Only after tests and bundle checks pass does the workflow publish an AppImage, its zsync metadata, a portable tarball, checksums, and upstream provenance to the fork. Both in-app feeds and AppImage differential-update metadata point to the fork. The official multi-platform release workflow is disabled on the fork to avoid competing releases. Upstream tags themselves remain unchanged.

The installed copy is upgraded once to this update-enabled build and automatic checks are enabled. The app checks shortly after launch and daily; its existing download, checksum verification, staging, and idle-restart policy remains in use. Original backups remain available. No extra access token, server or paid service is needed; the existing GitHub SSH login can install the workflows and GITHUB_TOKEN handles scheduled repository operations.

Verification covers merge preservation/conflicts, duplicate release suppression, version ordering, translation placeholders, the full QtTest suite, AppImage extraction and startup, and the live fork release feed. New upstream UI text requires translation before a later release can pass the catalog gate.
